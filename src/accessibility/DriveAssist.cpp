#include "DriveAssist.h"

#include "AudioCueService.h"
#include "ScreenReaderService.h"
#include "AccessibilityStrings.h"
#include "AccessibilityCVars.h"

#include <libultraship.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>

extern "C" {
#include <common_structs.h> // Player
#include <waypoints.h>      // gPathExpectedRotation + per-player path globals
// Signed lateral position on the track: -1 = left edge, 0 = center, +1 = right
// edge (magnitude > 1 once off the track). Defined in code_80005FD0.c, where the
// game uses it to keep the AI on the racing line.
f32 calculate_track_position_factor(f32 posX, f32 posZ, u16 waypointIndex, s32 pathIndex);
}

extern "C" {
extern Player* gPlayerOne;
// Laterally pans the player's own kart audio (engine). Defined in audio/external.c.
void Accessibility_SetKartAudioPan(float pan);
// Heading from point a to point b in the game's units: atan2s(b.x-a.x, b.z-a.z).
// Negate it to get a kart/path heading (the convention rotation[1] and
// gPathExpectedRotation use). Defined in racing/math_util.c.
s32 get_angle_between_two_vectors(f32* a, f32* b);
// Nonzero on mirror-mode tracks, where left and right are flipped. code_800029B0.h.
extern s32 gIsMirrorMode;
}

using namespace AccessibilityStrings;

namespace {

// Engine pan models (CVAR_ACCESS_DRIVE_PAN_MODE), selectable in the menu.
constexpr int kPanModeCurve = 0;      // heading error vs the path ahead: lean toward the upcoming curve
constexpr int kPanModeRacingLine = 1; // pure pursuit to a look-ahead point: also recenters onto the line

// Angle units are s16 binary angles: 0x10000 == 360 degrees (~182 per degree).
constexpr int kSteerDeadzone = 0x0300; // ~4 deg: within this the engine stays centered
constexpr int kSteerPanFull = 0x2000;  // ~45 deg: full lean beyond this (continuous)

// Edge-proximity cue on the lateral position factor (0 = center, 1 = edge/off-track).
// It is SILENT while you are comfortably centered (within the onset), then beeps
// once you drift past the onset toward an edge - faster and higher-pitched the
// closer you get, panning toward that edge - and becomes a steady held tone at
// kEdgeSolid (the edge). The onset is set by the Edge Sensitivity slider: it slides
// between kEdgeOnsetFar (only warns when very close) and kEdgeOnsetNear (warns from
// further in). Proximity p (remapped onset..solid) drives the rate and the pitch.
// Tuned to measured telemetry: the real road edge is at |factor| ~= 1.0, and normal
// (centered) driving averages ~0.59, so the onset band sits high to keep the centre
// genuinely quiet and only warn in the outer part of the lane.
constexpr float kEdgeOnsetFar = 0.95f;     // sensitivity 0: silent until right at the edge
constexpr float kEdgeOnsetNear = 0.45f;    // sensitivity 100: starts ~halfway out
constexpr float kEdgeSolid = 1.05f;        // at/just past the edge: constant held tone
constexpr float kEdgePan = 0.90f;          // max pan toward the edge (reached at the edge)
constexpr float kEdgeTonePitch = 1.80f;    // pitch of the held constant tone (the peak)
constexpr float kEdgeBeepPitchMin = 0.80f; // beep pitch just past the onset
constexpr float kEdgeBeepPitchMax = 1.80f; // beep pitch just before the held tone (matches the tone)
constexpr int kEdgeIntervalFar = 12;       // ticks between beeps just past the onset (slow)
constexpr int kEdgeIntervalNear = 3;       // ticks between beeps right before the held tone (fast, still discrete)

constexpr int kProbe = 4;          // window (points) for measuring local turn
constexpr int kCurveOn = 1800;     // net turn over kProbe that counts as entering a curve
constexpr int kCurveOff = 1100;    // ... and must drop below this to count as exited (hysteresis)
constexpr int kHardTurn = 5000;    // net turn over kProbe that counts as a hard curve
constexpr int kScan = 18;          // points ahead to search for the next curve
constexpr int kAnnounceDist = 12;  // announce when the entry is within this many points

// Sign convention. In this game's heading units (atan2s + the
// -get_angle_between_two_vectors convention used for every heading) a RIGHT turn
// comes out as a NEGATIVE signed angle. This one constant ties together the spoken
// turn direction AND the engine pan side - flip it if BOTH come out reversed in play.
constexpr bool kRightTurnIsNegative = true;

inline bool TurnIsRight(int angle) {
    return kRightTurnIsNegative ? (angle < 0) : (angle > 0);
}

// Engine pan (+1 = right ear) for a signed steer error: lean the sound toward the
// side you must steer, so the player drives TOWARD the sound (Forza-style).
inline float SteerPan(int angle, float scale) {
    const float mag = std::clamp(std::abs(angle) / scale, 0.0f, 1.0f);
    return TurnIsRight(angle) ? mag : -mag;
}

// Net signed heading change over w points starting at point p (s16 wraparound
// gives the shortest signed turn).
int16_t NetTurn(const int16_t* rot, int count, int p, int w) {
    const int a = rot[((p % count) + count) % count];
    const int b = rot[(((p + w) % count) + count) % count];
    return static_cast<int16_t>(b - a);
}

const char* TurnLabel(int16_t turn) {
    const bool right = TurnIsRight(turn);
    const bool hard = std::abs(static_cast<int>(turn)) >= kHardTurn;
    if (hard) {
        return right ? TURN_HARD_RIGHT : TURN_HARD_LEFT;
    }
    return right ? TURN_RIGHT : TURN_LEFT;
}

} // namespace

void DriveAssist::Reset() {
    Accessibility_SetKartAudioPan(0.0f);                        // recenter the kart engine audio
    AudioCueService::Instance().SetEdgeTone(false, 0.0f, 0.0f); // silence any held edge tone
    mCurveAnnounced = false;
    mApproachBeeps = 0;
    mWasInCurve = false;
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
    const int16_t* rotPath = gPathExpectedRotation[pathIndex];
    if (count <= 0 || rotPath == nullptr) {
        Accessibility_SetKartAudioPan(0.0f);
        return;
    }
    const int nearest = gNearestPathPointByPlayerId[playerId];

    // Signed lateral position on the track (-1 left edge .. 0 center .. +1 right
    // edge), the same quantity the game computes for the AI. Drives the edge cue.
    // Telemetry over a full lap confirmed the real road edge sits at |factor| ~= 1.0
    // (on-road averaged 0.59 and topped out ~1.2; off-road surfaces began ~1.03).
    const float lateralFactor = std::clamp(
        calculate_track_position_factor(player->pos[0], player->pos[2],
                                        static_cast<uint16_t>(nearest), pathIndex),
        -2.0f, 2.0f);

    // --- Layer 4: Steering Guide (pan the player's engine toward where to steer) ---
    // The sound leans toward the side you must steer to follow the racing line, so
    // the player drives TOWARD the sound (Forza Steering Guide semantics).
    {
        const int panMode = CVarGetInteger(CVAR_ACCESS_DRIVE_PAN_MODE, CVAR_ACCESS_DRIVE_PAN_MODE_DEFAULT);
        // User-tunable scale (0..1) so the lean can be softened to taste.
        const float strength = std::clamp(
            CVarGetInteger(CVAR_ACCESS_DRIVE_PAN_STRENGTH, CVAR_ACCESS_DRIVE_PAN_STRENGTH_DEFAULT) / 100.0f,
            0.0f, 1.0f);
        // Look-ahead ("anticipation"): how many path points ahead to aim. Smaller =
        // tighter centering (reacts to drift sooner); larger = smoother / leans into
        // curves earlier.
        const int lookAhead = std::clamp(
            CVarGetInteger(CVAR_ACCESS_DRIVE_LOOKAHEAD, CVAR_ACCESS_DRIVE_LOOKAHEAD_DEFAULT), 1, 30);
        // A steer error (toward where to go) drives the pan, so you drive TOWARD the
        // sound. The two models differ only in how that error is found.
        const int aheadIdx = (nearest + lookAhead) % count;
        int16_t error;
        if (panMode == kPanModeCurve) {
            // Curve direction: heading error vs the path ahead. Leans into the upcoming
            // curve but does NOT correct lateral drift.
            error = static_cast<int16_t>(rotPath[aheadIdx] - player->rotation[1]);
        } else {
            // Racing line / pure pursuit: aim at a point ahead on the line and steer
            // by (bearing - heading). Mirrors the game's own AI (code_80005FD0.c:1900)
            // - recenters onto the line AND anticipates the curve.
            const TrackPathPoint* tgt = &gTrackPaths[pathIndex][aheadIdx];
            f32 self[3] = { player->pos[0], player->pos[1], player->pos[2] };
            f32 target[3] = { static_cast<f32>(tgt->x), static_cast<f32>(tgt->y), static_cast<f32>(tgt->z) };
            const int16_t bearing = static_cast<int16_t>(-get_angle_between_two_vectors(self, target));
            error = static_cast<int16_t>(bearing - player->rotation[1]);
        }

        float pan = 0.0f;
        if (std::abs(static_cast<int>(error)) >= kSteerDeadzone) {
            pan = SteerPan(error, static_cast<float>(kSteerPanFull));
        }
        pan *= strength;
        if (gIsMirrorMode != 0) {
            pan = -pan; // mirror-mode tracks flip left/right
        }
        if (CVarGetInteger(CVAR_ACCESS_DRIVE_INVERT, CVAR_ACCESS_DRIVE_INVERT_DEFAULT) != 0) {
            pan = -pan;
        }
        // Low-pass filter so the pan eases toward the target instead of jumping.
        constexpr float kSmooth = 0.15f;
        mSmoothedPan += (pan - mSmoothedPan) * kSmooth;
        Accessibility_SetKartAudioPan(mSmoothedPan);
    }

    // --- Layers 1 & 2: find the upcoming curve, announce it, approach beeps ---
    int entryDist = -1;
    int16_t entryTurn = 0;
    for (int i = 1; i <= kScan; ++i) {
        const int16_t t = NetTurn(rotPath, count, nearest + i, kProbe);
        if (std::abs(static_cast<int>(t)) >= kCurveOn) {
            entryDist = i;
            entryTurn = t;
            break;
        }
    }

    if (entryDist != -1) {
        if (!mCurveAnnounced && entryDist <= kAnnounceDist) {
            mCurveAnnounced = true;
            mApproachBeeps = 0;
            reader.Speak(TurnLabel(entryTurn), true);
        }
        static const int kThresholds[3] = { 10, 6, 3 };
        static const float kPitches[3] = { 1.0f, 1.25f, 1.55f };
        if (mCurveAnnounced && mApproachBeeps < 3 && entryDist <= kThresholds[mApproachBeeps]) {
            AudioCueService::Instance().PlayBeep(CueBeep::Approach, kPitches[mApproachBeeps]);
            mApproachBeeps++;
        }
    } else {
        // No curve within range: ready to announce the next one.
        mCurveAnnounced = false;
        mApproachBeeps = 0;
    }

    // --- Layer 3: curve-progress beeps (entry / exit), from the local turn ---
    // Hysteresis (enter at kCurveOn, only leave below kCurveOff) so a turn value
    // hovering around the threshold doesn't flip in/out every frame and spam beeps.
    const int16_t localTurn = NetTurn(rotPath, count, nearest, kProbe);
    const int localMag = std::abs(static_cast<int>(localTurn));
    const bool inCurve = mWasInCurve ? (localMag >= kCurveOff) : (localMag >= kCurveOn);
    if (inCurve && !mWasInCurve) {
        AudioCueService::Instance().PlayBeep(CueBeep::Curve, 1.0f); // entry
    } else if (!inCurve && mWasInCurve) {
        AudioCueService::Instance().PlayBeep(CueBeep::Curve, 1.5f); // exit (higher pitch)
    }
    mWasInCurve = inCurve;

    // --- Layer 5: edge-proximity cue (silent when centered, scales toward the edge) ---
    if (CVarGetInteger(CVAR_ACCESS_EDGE_CUE, CVAR_ACCESS_EDGE_CUE_DEFAULT) != 0) {
        const float mag = std::abs(lateralFactor);
        // Sensitivity 0..100 picks the onset: how far toward the edge you must drift
        // before the cue starts. Below the onset (comfortably centered) it is SILENT.
        const float sens = std::clamp(
            CVarGetInteger(CVAR_ACCESS_EDGE_SENSITIVITY, CVAR_ACCESS_EDGE_SENSITIVITY_DEFAULT) / 100.0f,
            0.0f, 1.0f);
        const float onset = kEdgeOnsetFar + sens * (kEdgeOnsetNear - kEdgeOnsetFar);
        if (mag <= onset) {
            // Comfortably centered: silent.
            AudioCueService::Instance().SetEdgeTone(false, 0.0f, 0.0f);
            mEdgeBeepTimer = 0;
        } else {
            // p: 0 just past the onset .. 1 at the edge. Drives the beep rate AND
            // pitch; panned toward the edge being approached. Position info, so it
            // ignores the engine-pan invert option.
            const float p = std::clamp((mag - onset) / (kEdgeSolid - onset), 0.0f, 1.0f);
            const float side = std::clamp(lateralFactor, -1.0f, 1.0f) * kEdgePan;
            if (mag >= kEdgeSolid) {
                // Right at the limit: a steady held tone until pulled back inside.
                AudioCueService::Instance().SetEdgeTone(true, kEdgeTonePitch, side);
                mEdgeBeepTimer = 0;
            } else {
                // Approaching: beeps that get faster and higher the closer the edge.
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
