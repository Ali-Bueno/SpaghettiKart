#include "DriveAssist.h"

#include "AudioCueService.h"
#include "ScreenReaderService.h"
#include "AccessibilityStrings.h"
#include "AccessibilityCVars.h"

#include <libultraship.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

extern "C" {
#include <common_structs.h> // Player
#include <waypoints.h>      // path globals (gTrackPaths/Left/Right, gPathExpectedRotation, ...)
// Signed lateral position on the track: -1 = left edge, 0 = center, +1 = right edge
// (magnitude > 1 once off the track). Defined in code_80005FD0.c.
f32 calculate_track_position_factor(f32 posX, f32 posZ, u16 waypointIndex, s32 pathIndex);
// Signed track curvature the game itself uses to classify each section (analyze_track_section
// in code_80005FD0.c): POSITIVE = right, NEGATIVE = left, ~0 = straight. It is normalized by
// the chord lengths, so unlike a raw heading delta it is robust to the non-uniform spacing of
// the path points - which is exactly why we segment curves with it. Defined in code_80005FD0.c.
f32 calculate_track_curvature(s32 pathIndex, u16 waypointIndex);
}

extern "C" {
extern Player* gPlayerOne;
// Laterally pans the player's own kart audio (engine). Defined in audio/external.c.
void Accessibility_SetKartAudioPan(float pan);
// Heading from point a to point b in the game's units: atan2s(b.x-a.x, b.z-a.z). Negate it to
// get a kart/path heading (the convention rotation[1] and gPathExpectedRotation use).
s32 get_angle_between_two_vectors(f32* a, f32* b);
// Nonzero on mirror-mode tracks, where left and right are flipped.
extern s32 gIsMirrorMode;
}

using namespace AccessibilityStrings;

namespace {

// s16 binary angles: 0x10000 == 360 degrees.
constexpr float kS16ToDeg = 360.0f / 65536.0f;

// --- Steering guide (engine pan) -------------------------------------------------------
constexpr int kSteerDeadzone = 0x0300; // ~4 deg: within this the engine pan stays centered
constexpr int kSteerPanFull = 0x2000;  // ~45 deg: full lean beyond this
constexpr float kPanSmooth = 0.15f;    // low-pass factor for the engine pan

// --- Curve detection (built once per track, then driven by world distance) -------------
constexpr float kCurveEnterCurvature = 0.045f; // |signed curvature| above this is "curving"
constexpr int kCurveGapPoints = 3;             // straight points tolerated inside one curve
constexpr int kCurveMinPoints = 1;             // shortest curve kept, in points
constexpr float kCurveMinAngleDeg = 15.0f;     // shortest curve kept, in total heading change

// Severity grading, primarily from the PEAK curvature in the curve (the game's own measure:
// it classifies |curvature| > 0.10 as a strong curve). Grading by the PEAK - not an average -
// is what catches a tight section hidden inside an otherwise gentle curve, which was being
// mis-called "easy" (and crashed into). Total heading change promotes a strong sustained turn
// to a hairpin.
constexpr float kStrongCurv = 0.10f;       // peak >= this: at least "Hard" (tight)
constexpr float kModerateCurv = 0.07f;     // peak >= this (but below strong): "Normal"; below: "Easy"
constexpr float kHairpinAngleDeg = 120.0f; // a strong curve turning this much overall: "Hairpin"
constexpr float kLongWidths = 9.0f;        // arc length >= this * track width => "Long"

// Announce / approach / clearance distances, expressed in average-point-spacings so the
// feel matches the previous point-based logic but is uniform regardless of local density.
constexpr float kAnnouncePts = 12.0f;
constexpr float kApproachPts[3] = { 10.0f, 6.0f, 3.0f };
constexpr float kApproachPitch[3] = { 1.0f, 1.25f, 1.55f };
constexpr float kClearPts = 3.0f; // points past a curve's exit before it stops being "active"

// Series ("chicane") handling: if the gap between one curve's exit and the next curve's entry
// is shorter than this, there is effectively no straight between them, so they are announced
// together as one call ("hard left then easy right"). Up to kMaxChain curves are chained so the
// call stays short.
constexpr float kChainGapPts = 5.0f;
constexpr int kMaxChain = 3;

// In-curve traversal beeps: entry and apex share a pitch, the exit is higher to mark the end.
constexpr float kCurveEntryPitch = 1.0f;
constexpr float kCurveApexPitch = 1.0f;
constexpr float kCurveExitPitch = 1.5f;
// calculate_track_curvature looks ~3 points ahead (it spans points i..i+5), so the curvature
// run sits a little BEFORE the curve you actually feel. Shift the stored landmarks forward by
// this many points so the entry/apex/exit beeps line up with the real geometry.
constexpr int kCurvatureLead = 3;

// --- Edge-proximity cue (on the lateral position factor; 0 = center, 1 = edge) ----------
// Silent while comfortably centered, then beeps faster/higher toward an edge and becomes a
// held tone at the limit. Tuned to telemetry: the real road edge is at |factor| ~= 1.0 and
// centered driving averages ~0.59, so the onset band sits high to keep the centre quiet.
constexpr float kEdgeOnsetFar = 1.00f;
constexpr float kEdgeOnsetNear = 0.60f;
constexpr float kEdgeSolid = 1.08f;
constexpr float kEdgePan = 0.90f;
constexpr float kEdgeTonePitch = 1.80f;
constexpr float kEdgeBeepPitchMin = 0.80f;
constexpr float kEdgeBeepPitchMax = 1.80f;
constexpr int kEdgeIntervalFar = 12;
constexpr int kEdgeIntervalNear = 3;

// Sign convention. In this game's heading units a RIGHT turn is a NEGATIVE signed angle.
// This ties the engine-pan side to the steer direction; flip it if the pan comes out
// reversed in play. (The spoken curve direction comes from the curvature sign instead -
// positive = right - which is the OPPOSITE convention, handled where the curve is built.)
constexpr bool kRightTurnIsNegative = true;

inline bool TurnIsRight(int angle) {
    return kRightTurnIsNegative ? (angle < 0) : (angle > 0);
}

// Engine pan (+1 = right ear) for a signed steer error: lean toward the side to steer.
inline float SteerPan(int angle, float scale) {
    const float mag = std::clamp(std::abs(angle) / scale, 0.0f, 1.0f);
    return TurnIsRight(angle) ? mag : -mag;
}

// Signed forward distance from `from` to `to` along the looping path, in path points, range
// (-count/2, count/2]. Used for the in-curve traversal landmarks (index-based and proven).
int FwdDist(int from, int to, int count) {
    int d = ((to - from) % count + count) % count;
    if (d > count / 2) {
        d -= count;
    }
    return d;
}

} // namespace

float DriveAssist::ArcForward(int from, int to) const {
    if (mCumDist.size() < 2) {
        return 0.0f;
    }
    const float total = mCumDist.back();
    if (total <= 0.0f) {
        return 0.0f;
    }
    float d = std::fmod(mCumDist[to] - mCumDist[from], total);
    if (d < 0.0f) {
        d += total;
    }
    if (d > total * 0.5f) {
        d -= total;
    }
    return d;
}

// Walk the whole path once, segment it into curves using the game's normalized curvature,
// and grade each one from its geometry. Cheap (O(point count)); done only when the track or
// the player's path index changes.
void DriveAssist::RebuildCurveMap(int pathIndex, int count) {
    mCurves.clear();
    mAnnounced.clear();
    mCumDist.assign(count + 1, 0.0f);
    mMapKey = (count > 0) ? static_cast<const void*>(gTrackPaths[pathIndex]) : nullptr;
    mMapPathIndex = pathIndex;
    mMapCount = count;
    mAvgSpacing = 1.0f;
    if (count < 8) {
        return; // not a real racing path (battle arenas have none)
    }

    const TrackPathPoint* path = gTrackPaths[pathIndex];
    const TrackPathPoint* left = gTrackLeftPaths[pathIndex];
    const TrackPathPoint* right = gTrackRightPaths[pathIndex];
    const int16_t* rot = gPathExpectedRotation[pathIndex];

    // Cumulative arc length (closing the loop) and the mean spacing used to anchor distances.
    for (int i = 0; i < count; ++i) {
        const int n = (i + 1) % count;
        const float dx = static_cast<float>(path[n].x - path[i].x);
        const float dz = static_cast<float>(path[n].z - path[i].z);
        mCumDist[i + 1] = mCumDist[i] + std::sqrt(dx * dx + dz * dz);
    }
    const float total = mCumDist[count];
    mAvgSpacing = (total > 0.0f) ? (total / count) : 1.0f;

    // Curvature sign per point (deadzoned), and the raw curvature for the apex search.
    std::vector<int8_t> sgn(count);
    std::vector<float> curv(count);
    for (int i = 0; i < count; ++i) {
        const float c = calculate_track_curvature(pathIndex, static_cast<u16>(i));
        curv[i] = c;
        sgn[i] = (c > kCurveEnterCurvature) ? 1 : (c < -kCurveEnterCurvature) ? -1 : 0;
    }

    // Start scanning from a straight point so no curve is split by the array boundary. (If the
    // entire loop curves - very rare for real tracks - a curve straddling index 0 may split;
    // accepted.)
    int anchor = 0;
    for (int i = 0; i < count; ++i) {
        if (sgn[i] == 0) {
            anchor = i;
            break;
        }
    }

    auto addCurve = [&](int entry, int exit) {
        const int steps = FwdDist(entry, exit, count); // forward, >= 0
        if (steps + 1 < kCurveMinPoints) {
            return;
        }
        // Total heading change = summed per-step deltas (signed, so a > 180 deg hairpin is not
        // shortest-angle-clipped) - used only as a magnitude. The direction comes from the
        // curvature sum instead: the heading-delta sign proved unreliable here (opposite to the
        // real bend), whereas the game's curvature is reliable for direction - POSITIVE = right.
        int turnSum = 0;
        float curvSum = 0.0f;
        float peak = 0.0f; // tightest point in the curve (drives the severity grade)
        for (int k = 0; k <= steps; ++k) {
            const int idx = (entry + k) % count;
            curvSum += curv[idx];
            peak = std::max(peak, std::abs(curv[idx]));
            if (k < steps) {
                const int nxt = (idx + 1) % count;
                turnSum += static_cast<int16_t>(rot[nxt] - rot[idx]);
            }
        }
        const float angleDeg = std::abs(turnSum) * kS16ToDeg;
        if (angleDeg < kCurveMinAngleDeg) {
            return; // a wiggle, not a curve worth calling
        }

        // The apex/middle beep marks the geometric MIDDLE of the curve (more interpretable than
        // the tightest point, which can sit right at the entry or exit).
        const int mid = (entry + steps / 2) % count;

        // Severity from the PEAK curvature so a tight section inside the curve is not averaged
        // away. A strong sustained turn (big total angle) is a hairpin.
        Severity sev;
        if (peak >= kStrongCurv) {
            sev = (angleDeg >= kHairpinAngleDeg) ? Severity::Hairpin : Severity::Hard;
        } else if (peak >= kModerateCurv) {
            sev = Severity::Normal;
        } else {
            sev = Severity::Easy;
        }

        // Arc length and track width (at the curve's middle) for the "long" modifier.
        float arc = std::fmod(mCumDist[exit] - mCumDist[entry], total);
        if (arc < 0.0f) {
            arc += total;
        }
        const float wdx = static_cast<float>(right[mid].x - left[mid].x);
        const float wdz = static_cast<float>(right[mid].z - left[mid].z);
        const float width = std::sqrt(wdx * wdx + wdz * wdz);

        Curve cv;
        // Shift the landmarks forward to compensate the curvature look-ahead (see kCurvatureLead)
        // so the beeps align with the felt entry / middle / exit.
        cv.entry = (entry + kCurvatureLead) % count;
        cv.apex = (mid + kCurvatureLead) % count;
        cv.exit = (exit + kCurvatureLead) % count;
        cv.right = (curvSum > 0.0f); // game curvature convention: positive = right
        cv.severity = sev;
        cv.isLong = (width > 1.0f) && (arc >= kLongWidths * width);
        mCurves.push_back(cv);
    };

    int runSign = 0, runStart = -1, runEnd = -1, gap = 0;
    auto closeRun = [&]() {
        if (runSign != 0 && runStart >= 0) {
            addCurve(runStart, runEnd);
        }
        runSign = 0;
        runStart = -1;
        gap = 0;
    };
    for (int j = 0; j < count; ++j) {
        const int idx = (anchor + j) % count;
        const int s = sgn[idx];
        if (runSign == 0) {
            if (s != 0) {
                runSign = s;
                runStart = idx;
                runEnd = idx;
                gap = 0;
            }
        } else if (s == runSign) {
            runEnd = idx;
            gap = 0;
        } else if (s == 0) {
            if (++gap > kCurveGapPoints) {
                closeRun();
            }
        } else { // opposite direction: an S-curve splits here
            closeRun();
            runSign = s;
            runStart = idx;
            runEnd = idx;
            gap = 0;
        }
    }
    closeRun();

    mAnnounced.assign(mCurves.size(), false); // none spoken yet for this map
}

// Spoken call for one curve: <severity prefix> + <direction> + optional " long".
std::string DriveAssist::CurvePhrase(const Curve& c) const {
    std::string s;
    switch (c.severity) {
        case Severity::Hairpin: s = TURN_PREFIX_HAIRPIN; break;
        case Severity::Hard:    s = TURN_PREFIX_HARD; break;
        case Severity::Easy:    s = TURN_PREFIX_EASY; break;
        case Severity::Normal:  break;
    }
    s += c.right ? TURN_RIGHT : TURN_LEFT;
    if (c.isLong) {
        s += TURN_SUFFIX_LONG;
    }
    return s;
}

void DriveAssist::Reset() {
    Accessibility_SetKartAudioPan(0.0f);
    AudioCueService::Instance().SetEdgeTone(false, 0.0f, 0.0f);
    mActiveCurve = -1;
    std::fill(mAnnounced.begin(), mAnnounced.end(), false);
    mLastLap = -1;
    mApproachBeeps = 0;
    mCurvePhase = 0;
    mEdgeBeepTimer = 0;
    mSmoothedPan = 0.0f;
}

void DriveAssist::Tick(ScreenReaderService& reader) {
    const Player* player = gPlayerOne;
    if (player == nullptr) {
        Accessibility_SetKartAudioPan(0.0f);
        return;
    }

    const int playerId = 0;
    const int pathIndex = gPathIndexByPlayerId[playerId];
    if (pathIndex < 0 || pathIndex >= 4) {
        Accessibility_SetKartAudioPan(0.0f);
        return;
    }
    const int count = gPathCountByPathIndex[pathIndex];
    if (count <= 0 || gPathExpectedRotation[pathIndex] == nullptr) {
        Accessibility_SetKartAudioPan(0.0f);
        return;
    }
    const int nearest = gNearestPathPointByPlayerId[playerId];

    // Rebuild the curve map when the track or the player's path (Yoshi Valley forks, Koopa
    // Troopa Beach split) changes.
    if (mMapKey != static_cast<const void*>(gTrackPaths[pathIndex]) || mMapPathIndex != pathIndex ||
        mMapCount != count) {
        RebuildCurveMap(pathIndex, count);
    }

    // Signed lateral position (-1 left edge .. 0 center .. +1 right edge). Drives the edge cue
    // and the centering term.
    const float lateralFactor = std::clamp(
        calculate_track_position_factor(player->pos[0], player->pos[2], static_cast<uint16_t>(nearest),
                                        pathIndex),
        -2.0f, 2.0f);

    // --- Layer 4: Steering Guide (pure pursuit toward the racing line) --------------------
    // Aim at a look-ahead point on the line and pan the engine toward the side to steer, so the
    // player drives TOWARD the sound (mirrors the game's own AI steering).
    {
        const float strength = std::clamp(
            CVarGetInteger(CVAR_ACCESS_DRIVE_PAN_STRENGTH, CVAR_ACCESS_DRIVE_PAN_STRENGTH_DEFAULT) / 100.0f,
            0.0f, 1.0f);
        // Look-ahead ("anticipation") in path points: how far ahead on the line to aim.
        const int lookAhead = std::clamp(
            CVarGetInteger(CVAR_ACCESS_DRIVE_LOOKAHEAD, CVAR_ACCESS_DRIVE_LOOKAHEAD_DEFAULT), 1, 30);
        const int aheadIdx = (nearest + lookAhead) % count;
        const TrackPathPoint* tgt = &gTrackPaths[pathIndex][aheadIdx];
        f32 self[3] = { player->pos[0], player->pos[1], player->pos[2] };
        f32 target[3] = { static_cast<f32>(tgt->x), static_cast<f32>(tgt->y), static_cast<f32>(tgt->z) };
        const int16_t bearing = static_cast<int16_t>(-get_angle_between_two_vectors(self, target));
        const int16_t error = static_cast<int16_t>(bearing - player->rotation[1]);

        float pan = 0.0f;
        if (std::abs(static_cast<int>(error)) >= kSteerDeadzone) {
            pan = SteerPan(error, static_cast<float>(kSteerPanFull));
        }
        pan *= strength;
        if (gIsMirrorMode != 0) {
            pan = -pan; // mirror tracks flip left/right
        }
        if (CVarGetInteger(CVAR_ACCESS_DRIVE_INVERT, CVAR_ACCESS_DRIVE_INVERT_DEFAULT) != 0) {
            pan = -pan;
        }
        mSmoothedPan += (pan - mSmoothedPan) * kPanSmooth;
        Accessibility_SetKartAudioPan(mSmoothedPan);
    }

    // --- Layers 1-3: curve announcement, approach beeps, in-curve traversal beeps ----------
    // Everything keys off the prebuilt curve map and real distances, so detection is stable and
    // each curve is announced once per lap (re-armed below) - the same call on every lap.
    if (!mCurves.empty()) {
        // Pick the curve we are approaching or driving: the nearest-ahead curve whose exit we
        // have not yet cleared. Curves fully behind are excluded by the clearance test.
        const float clearDist = kClearPts * mAvgSpacing;
        int active = -1;
        float bestEntry = 1e18f;
        for (size_t i = 0; i < mCurves.size(); ++i) {
            if (ArcForward(nearest, mCurves[i].exit) <= -clearDist) {
                continue; // finished this one
            }
            const float dEntry = ArcForward(nearest, mCurves[i].entry);
            if (dEntry < bestEntry) {
                bestEntry = dEntry;
                active = static_cast<int>(i);
            }
        }

        // Re-arm announcements each lap so a curve called on lap 1 is called again on lap 2.
        const int lap = player->lapCount;
        if (lap != mLastLap) {
            mLastLap = lap;
            std::fill(mAnnounced.begin(), mAnnounced.end(), false);
        }
        // New active curve: reset its approach/traversal progress.
        if (active != mActiveCurve) {
            mActiveCurve = active;
            mApproachBeeps = 0;
            mCurvePhase = 0;
        }

        if (active >= 0) {
            const Curve& cv = mCurves[active];
            const float dEntry = ArcForward(nearest, cv.entry);
            // A "chain follower" is a curve linked to the one before it (no straight between). The
            // series was already announced and the approach already counted down at its FIRST
            // curve, so a follower gets only its own in-curve beeps - no fresh approach countdown.
            const bool chainFollower =
                active > 0 && ArcForward(mCurves[active - 1].exit, cv.entry) <= kChainGapPts * mAvgSpacing;

            // 1. Speak the graded call once (per lap), when the entry comes within announce
            //    distance. Back-to-back curves with no straight between them are chained into one
            //    call to anticipate the series, e.g. "hard left then easy right".
            if (active < static_cast<int>(mAnnounced.size()) && !mAnnounced[active] &&
                dEntry <= kAnnouncePts * mAvgSpacing) {
                std::string phrase = CurvePhrase(cv);
                mAnnounced[active] = true;
                int last = active;
                for (int n = 1; n < kMaxChain; ++n) {
                    const int j = last + 1;
                    if (j >= static_cast<int>(mCurves.size())) {
                        break; // don't chain across the start/finish line
                    }
                    // Linked only if the gap to the next curve is too short to be a real straight.
                    if (ArcForward(mCurves[last].exit, mCurves[j].entry) > kChainGapPts * mAvgSpacing) {
                        break;
                    }
                    phrase += TURN_CHAIN;
                    phrase += CurvePhrase(mCurves[j]);
                    mAnnounced[j] = true;
                    last = j;
                }
                reader.Speak(phrase, true);
            }

            // 2. Approach beeps: up to three, rising, as the entry nears - but only for the first
            //    curve of a series; a chain follower gets only its in-curve beeps.
            if (!chainFollower && mApproachBeeps < 3 && dEntry > 0.0f &&
                dEntry <= kApproachPts[mApproachBeeps] * mAvgSpacing) {
                AudioCueService::Instance().PlayBeep(CueBeep::Approach, kApproachPitch[mApproachBeeps]);
                mApproachBeeps++;
            }

            // 3. Traversal beeps at the curve's stored landmarks: entry, apex, higher-pitched exit.
            switch (mCurvePhase) {
                case 0:
                    if (FwdDist(nearest, cv.entry, count) <= 0) {
                        AudioCueService::Instance().PlayBeep(CueBeep::Curve, kCurveEntryPitch);
                        mCurvePhase = 1;
                    }
                    break;
                case 1:
                    if (FwdDist(nearest, cv.apex, count) <= 0) {
                        AudioCueService::Instance().PlayBeep(CueBeep::Curve, kCurveApexPitch);
                        mCurvePhase = 2;
                    }
                    if (FwdDist(nearest, cv.exit, count) <= 0) {
                        AudioCueService::Instance().PlayBeep(CueBeep::Curve, kCurveExitPitch);
                        mCurvePhase = 3;
                    }
                    break;
                case 2:
                    if (FwdDist(nearest, cv.exit, count) <= 0) {
                        AudioCueService::Instance().PlayBeep(CueBeep::Curve, kCurveExitPitch);
                        mCurvePhase = 3;
                    }
                    break;
                default:
                    break; // 3: done until the active curve changes
            }
        }
    }

    // --- Layer 5: edge-proximity cue (silent when centered, scales toward the edge) ---------
    if (CVarGetInteger(CVAR_ACCESS_EDGE_CUE, CVAR_ACCESS_EDGE_CUE_DEFAULT) != 0) {
        const float mag = std::abs(lateralFactor);
        const float sens = std::clamp(
            CVarGetInteger(CVAR_ACCESS_EDGE_SENSITIVITY, CVAR_ACCESS_EDGE_SENSITIVITY_DEFAULT) / 100.0f,
            0.0f, 1.0f);
        const float onset = kEdgeOnsetFar + sens * (kEdgeOnsetNear - kEdgeOnsetFar);
        if (mag <= onset) {
            AudioCueService::Instance().SetEdgeTone(false, 0.0f, 0.0f);
            mEdgeBeepTimer = 0;
        } else {
            const float p = std::clamp((mag - onset) / (kEdgeSolid - onset), 0.0f, 1.0f);
            const float side = std::clamp(lateralFactor, -1.0f, 1.0f) * kEdgePan;
            if (mag >= kEdgeSolid) {
                AudioCueService::Instance().SetEdgeTone(true, kEdgeTonePitch, side);
                mEdgeBeepTimer = 0;
            } else {
                AudioCueService::Instance().SetEdgeTone(false, 0.0f, 0.0f);
                if (mEdgeBeepTimer <= 0) {
                    const float pitch = kEdgeBeepPitchMin + p * (kEdgeBeepPitchMax - kEdgeBeepPitchMin);
                    AudioCueService::Instance().PlayBeep(CueBeep::Edge, pitch, side);
                    mEdgeBeepTimer = static_cast<int>(
                        kEdgeIntervalFar + (kEdgeIntervalNear - kEdgeIntervalFar) * p + 0.5f);
                } else {
                    --mEdgeBeepTimer;
                }
            }
        }
    } else {
        AudioCueService::Instance().SetEdgeTone(false, 0.0f, 0.0f);
    }
}
