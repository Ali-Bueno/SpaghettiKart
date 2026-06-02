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
}

using namespace AccessibilityStrings;

namespace {

// Engine pan models (CVAR_ACCESS_DRIVE_PAN_MODE).
constexpr int kPanModeLateral = 0; // pan by lateral lane position (Top Speed style)
constexpr int kPanModeHeading = 1; // pan by heading error (side to steer)

// Angle units are s16 binary angles: 0x10000 == 360 degrees (~182 per degree).
constexpr int kLookAhead = 3;
constexpr int kEngineDeadzone = 1500; // ~8 deg: within this, engine stays centered
constexpr int kEnginePanFull = 12000; // ~66 deg: full lean beyond this (continuous)

// Lateral-position model: within this fraction of center the engine stays put.
constexpr float kLateralDeadzone = 0.08f;
// Edge-proximity cue thresholds (hysteresis) on the lateral position factor.
constexpr float kEdgeWarn = 0.82f;  // fire the cue when this close to an edge
constexpr float kEdgeClear = 0.62f; // re-arm only after pulling back inside this

constexpr int kProbe = 4;          // window (points) for measuring local turn
constexpr int kCurveOn = 1800;     // net turn over kProbe that counts as a curve
constexpr int kHardTurn = 5000;    // net turn over kProbe that counts as a hard curve
constexpr int kScan = 18;          // points ahead to search for the next curve
constexpr int kAnnounceDist = 12;  // announce when the entry is within this many points

// Sign convention: is an increasing path heading a right turn? Flip this single
// constant if announcements and engine pan come out on the wrong side.
constexpr bool kPositiveAngleIsRight = true;

// Net signed heading change over w points starting at point p (s16 wraparound
// gives the shortest signed turn).
int16_t NetTurn(const int16_t* rot, int count, int p, int w) {
    const int a = rot[((p % count) + count) % count];
    const int b = rot[(((p + w) % count) + count) % count];
    return static_cast<int16_t>(b - a);
}

const char* TurnLabel(int16_t turn) {
    const bool right = (turn > 0) == kPositiveAngleIsRight;
    const bool hard = std::abs(static_cast<int>(turn)) >= kHardTurn;
    if (hard) {
        return right ? TURN_HARD_RIGHT : TURN_HARD_LEFT;
    }
    return right ? TURN_RIGHT : TURN_LEFT;
}

} // namespace

void DriveAssist::Reset() {
    Accessibility_SetKartAudioPan(0.0f); // recenter the kart engine audio
    mCurveAnnounced = false;
    mApproachBeeps = 0;
    mWasInCurve = false;
    mWasNearEdge = false;
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
    // edge), the same quantity the game computes for the AI. Drives pan mode 0
    // and the edge cue. Computed once and shared by both.
    const float lateralFactor = std::clamp(
        calculate_track_position_factor(player->pos[0], player->pos[2],
                                        static_cast<uint16_t>(nearest), pathIndex),
        -2.0f, 2.0f);

    // --- Layer 4: directional reference (pan the player's engine audio) ---
    {
        const int panMode = CVarGetInteger(CVAR_ACCESS_DRIVE_PAN_MODE, CVAR_ACCESS_DRIVE_PAN_MODE_DEFAULT);
        // User-tunable scale (0..1) so the lean can be softened to taste.
        const float strength = std::clamp(
            CVarGetInteger(CVAR_ACCESS_DRIVE_PAN_STRENGTH, CVAR_ACCESS_DRIVE_PAN_STRENGTH_DEFAULT) / 100.0f,
            0.0f, 1.0f);
        float pan = 0.0f;
        if (panMode == kPanModeHeading) {
            // Heading-error model: pan toward the side to steer so the path ahead
            // lines up with where the kart is pointing.
            const int idx = (nearest + kLookAhead) % count;
            const int16_t error = static_cast<int16_t>(rotPath[idx] - player->rotation[1]);
            if (std::abs(static_cast<int>(error)) >= kEngineDeadzone) {
                pan = std::clamp(static_cast<float>(error) / kEnginePanFull, -1.0f, 1.0f);
                if (!kPositiveAngleIsRight) {
                    pan = -pan;
                }
            }
        } else {
            // Lateral-position model (Top Speed): pan toward the edge you have
            // drifted to; centered = on the racing line. In a curve the line moves
            // under you, so the pan leans outward until you steer to follow it.
            // Quadratic response (the original Top Speed curve): gentle near the
            // center, only firm near the edges, so small wobbles at speed don't
            // slam the sound from side to side.
            if (std::abs(lateralFactor) >= kLateralDeadzone) {
                const float f = std::clamp(lateralFactor, -1.0f, 1.0f);
                pan = (f < 0.0f ? -1.0f : 1.0f) * f * f;
            }
        }
        pan *= strength;
        if (CVarGetInteger(CVAR_ACCESS_DRIVE_INVERT, CVAR_ACCESS_DRIVE_INVERT_DEFAULT) != 0) {
            pan = -pan;
        }
        // Low-pass filter so the pan eases toward the target instead of jumping.
        constexpr float kSmooth = 0.12f;
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
    const int16_t localTurn = NetTurn(rotPath, count, nearest, kProbe);
    const bool inCurve = std::abs(static_cast<int>(localTurn)) >= kCurveOn;
    if (inCurve && !mWasInCurve) {
        AudioCueService::Instance().PlayBeep(CueBeep::Curve, 1.0f); // entry
    } else if (!inCurve && mWasInCurve) {
        AudioCueService::Instance().PlayBeep(CueBeep::Curve, 1.5f); // exit
    }
    mWasInCurve = inCurve;

    // --- Layer 5: edge-proximity cue (about to run off the track) ---
    if (CVarGetInteger(CVAR_ACCESS_EDGE_CUE, CVAR_ACCESS_EDGE_CUE_DEFAULT) != 0) {
        const float mag = std::abs(lateralFactor);
        if (!mWasNearEdge && mag >= kEdgeWarn) {
            mWasNearEdge = true;
            // Pan the beep toward the edge being approached. This is a position
            // warning, so it is not affected by the engine-pan invert option.
            const float side = lateralFactor > 0.0f ? 0.85f : -0.85f;
            AudioCueService::Instance().PlayBeep(CueBeep::Edge, 1.0f, side);
        } else if (mWasNearEdge && mag <= kEdgeClear) {
            mWasNearEdge = false;
        }
    }
}
