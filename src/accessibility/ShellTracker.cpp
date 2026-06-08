#include "ShellTracker.h"

#include "AudioCueService.h"
#include "AccessibilityCVars.h"

#include <libultraship.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

extern "C" {
#include <common_structs.h> // Player
#include <actor_types.h>    // struct Actor / struct ShellActor, enum ActorType, enum ShellState
}

extern "C" {
extern Player* gPlayerOne;
// Actor list accessors (port/Game.cpp). Shells live in this list as ACTOR_GREEN_SHELL /
// ACTOR_RED_SHELL / ACTOR_BLUE_SPINY_SHELL entries, laid out as struct ShellActor.
size_t CM_GetActorSize(void);
struct Actor* CM_GetActor(size_t index);
// Heading from point a to point b: atan2s(b.x-a.x, b.z-a.z). Negate it to match the
// kart/path heading convention rotation[1] uses (racing/math_util.c).
s32 get_angle_between_two_vectors(f32* a, f32* b);
// Nonzero on mirror-mode tracks, where left and right are flipped (code_800029B0.h).
extern s32 gIsMirrorMode;
}

namespace {

// Angle units are s16 binary angles (0x10000 == 360 deg, ~182 per degree).
constexpr int kPanFullAngle = 0x2000;                     // ~45 deg off-heading: full lean to that side
constexpr float kS16ToRad = 3.14159265358979f / 32768.0f; // s16 binary angle -> radians (32768 == pi)

// A shell only counts as "thrown / in flight" once its horizontal speed passes this.
// The velocity field is zero until the shell is fired, and held / released / orbiting
// shells keep their position locked to a kart and never build velocity, so this cleanly
// excludes them. Flying shells travel at roughly 4-11 units/frame.
constexpr float kMovingSpeed = 1.5f;

// Detection range (world units). Shells are a fast threat, so this is generous - a homing
// red shell is heard from well behind. Kart top speed is ~9 units/frame.
constexpr float kRange = 800.0f;

constexpr float kVolumeNear = 0.90f; // shell right on top of you
constexpr float kVolumeFar = 0.30f;  // at the edge of the detection range

// Same Doppler "it is behind you" pitch drop as the item-box beacon: forward = cos(angle
// to the shell) is +1 dead ahead, 0 abeam, -1 directly behind; the pitch is left at 1.0
// while ahead and lowered only as the shell falls behind.
constexpr float kDopplerDrop = 0.30f; // pitch at directly behind = 1.0 - this (= 0.70)

// Keep the loop alive this many ticks after the last moving shell, so a one-frame velocity
// dip (a wall bounce, a homing state transition) does not click the loop off and back on.
constexpr int kHoldTicks = 8;

} // namespace

void ShellTracker::Reset() {
    mHoldTimer[0] = 0;
    mHoldTimer[1] = 0;
    AudioCueService::Instance().StopShellLoop();
    AudioCueService::Instance().StopShellRedLoop();
}

void ShellTracker::Tick() {
    if (CVarGetInteger(CVAR_ACCESS_SHELL_CUE, CVAR_ACCESS_SHELL_CUE_DEFAULT) == 0) {
        Reset();
        return;
    }
    const Player* player = gPlayerOne;
    if (player == nullptr) {
        Reset();
        return;
    }

    const float px = player->pos[0];
    const float pz = player->pos[2];
    const int16_t heading = player->rotation[1];

    // Track the nearest flying shell of each group separately: index 0 = green / blue,
    // index 1 = red. The red shell gets its own sound (it homes, so it is worth recognizing),
    // while green and blue share theirs. Both behave identically. While a shell is ahead the
    // cue points toward it; once it passes, the same shell keeps sounding from behind at a
    // lower pitch (the Doppler cue) until it dies or leaves range.
    float bestDist[2] = { kRange, kRange };
    int16_t bestError[2] = { 0, 0 };
    bool found[2] = { false, false };

    const size_t count = CM_GetActorSize();
    for (size_t i = 0; i < count; ++i) {
        struct Actor* actor = CM_GetActor(i);
        if (actor == nullptr || actor->flags == 0) {
            continue;
        }
        int group;
        if (actor->type == ACTOR_RED_SHELL) {
            group = 1; // red shell: its own distinct sound
        } else if (actor->type == ACTOR_GREEN_SHELL || actor->type == ACTOR_BLUE_SPINY_SHELL) {
            group = 0; // green / blue shell: shared sound
        } else {
            continue;
        }
        const struct ShellActor* shell = reinterpret_cast<const struct ShellActor*>(actor);
        if (shell->state == DESTROYED_SHELL) {
            continue; // already dead / bouncing out, not a live threat
        }
        // Skip shells still attached to a kart (held / orbiting): they never build velocity.
        const float vx = shell->velocity[0];
        const float vz = shell->velocity[2];
        if ((vx * vx + vz * vz) < (kMovingSpeed * kMovingSpeed)) {
            continue;
        }
        const float dx = shell->pos[0] - px;
        const float dz = shell->pos[2] - pz;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist >= bestDist[group]) {
            continue;
        }
        f32 self[3] = { px, player->pos[1], pz };
        f32 shellPos[3] = { shell->pos[0], shell->pos[1], shell->pos[2] };
        const int16_t bearing = static_cast<int16_t>(-get_angle_between_two_vectors(self, shellPos));
        bestError[group] = static_cast<int16_t>(bearing - heading);
        bestDist[group] = dist;
        found[group] = true;
    }

    DriveVoice(false, found[0], bestDist[0], bestError[0]); // green / blue
    DriveVoice(true, found[1], bestDist[1], bestError[1]);  // red
}

void ShellTracker::DriveVoice(bool red, bool found, float dist, int errorAngle) {
    AudioCueService& audio = AudioCueService::Instance();
    const int idx = red ? 1 : 0;

    if (!found) {
        // Brief grace period so a transient velocity dip does not click the loop off; the
        // loop keeps playing with its last pan/pitch/volume until the hold elapses.
        if (mHoldTimer[idx] > 0) {
            --mHoldTimer[idx];
            return;
        }
        if (red) {
            audio.StopShellRedLoop();
        } else {
            audio.StopShellLoop();
        }
        return;
    }
    mHoldTimer[idx] = kHoldTicks;

    // Pan toward the shell. Convention (matches the steering guide / item-box beacon): a
    // target to the RIGHT is a negative signed angle, and positive pan = right ear.
    float pan = std::clamp(-static_cast<float>(errorAngle) / static_cast<float>(kPanFullAngle), -1.0f, 1.0f);
    if (gIsMirrorMode != 0) {
        pan = -pan; // mirror-mode tracks flip left/right
    }
    // Louder as the shell gets closer (position info, so it ignores the steering invert).
    const float t = std::clamp(dist / kRange, 0.0f, 1.0f);
    const float volume = kVolumeNear + t * (kVolumeFar - kVolumeNear);
    // Lower the pitch once the shell falls behind the kart (forward < 0): a Doppler cue.
    const float forward = std::cos(static_cast<float>(errorAngle) * kS16ToRad);
    const float pitch = 1.0f + kDopplerDrop * std::min(0.0f, forward);

    if (red) {
        audio.SetShellRedLoop(true, pan, volume, pitch);
    } else {
        audio.SetShellLoop(true, pan, volume, pitch);
    }
}
