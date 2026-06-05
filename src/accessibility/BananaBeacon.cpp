#include "BananaBeacon.h"

#include "AudioCueService.h"
#include "AccessibilityCVars.h"

#include <libultraship.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

extern "C" {
#include <common_structs.h> // Player
#include <actor_types.h>    // struct Actor / struct BananaActor, enum ActorType, enum BananaState
}

extern "C" {
extern Player* gPlayerOne;
// Actor list accessors (port/Game.cpp). Bananas live in this list as ACTOR_BANANA entries
// (single bananas and dropped bunch bananas alike), laid out as struct BananaActor.
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

// Detection range (world units) chosen by the Banana range slider, same mapping as the
// item-box beacon. Kart top speed is ~9 units/frame, so this band gives roughly half a
// second (close) to a couple of seconds (far) of warning before you reach the banana.
constexpr float kRangeMin = 100.0f; // slider 0: only warns when nearly on top of it
constexpr float kRangeMax = 600.0f; // slider 100: warns from well ahead

constexpr float kVolumeNear = 0.90f; // banana right in front of you
constexpr float kVolumeFar = 0.30f;  // at the edge of the detection range

// Same Doppler "it is behind you" pitch drop as the item-box beacon: forward = cos(angle
// to the banana) is +1 dead ahead, 0 abeam, -1 directly behind; the pitch is left at 1.0
// while ahead and lowered only once the banana falls behind (you have passed it safely).
constexpr float kDopplerDrop = 0.30f; // pitch at directly behind = 1.0 - this (= 0.70)

// The blip repeats every CVAR_ACCESS_BANANA_INTERVAL milliseconds (a player slider); the
// beacon is ticked once per ~30 fps game frame, so convert ms -> ticks. Default 600 ms = 18.
constexpr int kGameTicksPerSecond = 30;

} // namespace

void BananaBeacon::Reset() {
    mBlipTimer = 0;
    AudioCueService::Instance().StopBananaBeacon();
}

void BananaBeacon::Tick() {
    if (CVarGetInteger(CVAR_ACCESS_BANANA_CUE, CVAR_ACCESS_BANANA_CUE_DEFAULT) == 0) {
        Reset();
        return;
    }
    const Player* player = gPlayerOne;
    if (player == nullptr) {
        Reset();
        return;
    }

    const float sens = std::clamp(
        CVarGetInteger(CVAR_ACCESS_BANANA_RANGE, CVAR_ACCESS_BANANA_RANGE_DEFAULT) / 100.0f, 0.0f, 1.0f);
    const float range = kRangeMin + sens * (kRangeMax - kRangeMin);

    const float px = player->pos[0];
    const float pz = player->pos[2];
    const int16_t heading = player->rotation[1];

    // Pick the nearest grounded banana, in any direction. While it is ahead the blip warns
    // you toward it so you can steer clear; once you pass it the same banana keeps sounding
    // from behind at a lower pitch (the Doppler cue) until it leaves range.
    float bestDist = range;
    int16_t bestError = 0;
    bool found = false;
    const size_t count = CM_GetActorSize();
    for (size_t i = 0; i < count; ++i) {
        struct Actor* actor = CM_GetActor(i);
        if (actor == nullptr || actor->flags == 0) {
            continue;
        }
        if (actor->type != ACTOR_BANANA) {
            continue;
        }
        const struct BananaActor* banana = reinterpret_cast<const struct BananaActor*>(actor);
        if (banana->state != BANANA_ON_GROUND) {
            continue; // skip held / trailing / in-air / dying bananas: only grounded ones are a hazard
        }
        const float dx = banana->pos[0] - px;
        const float dz = banana->pos[2] - pz;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist >= bestDist) {
            continue;
        }
        f32 self[3] = { px, player->pos[1], pz };
        f32 bananaPos[3] = { banana->pos[0], banana->pos[1], banana->pos[2] };
        const int16_t bearing = static_cast<int16_t>(-get_angle_between_two_vectors(self, bananaPos));
        bestError = static_cast<int16_t>(bearing - heading);
        bestDist = dist;
        found = true;
    }

    if (!found) {
        Reset();
        return;
    }

    // Pan toward the banana so you can steer clear. Convention (matches the steering guide
    // / item-box beacon): a target to the RIGHT is a negative signed angle, positive pan =
    // right ear.
    float pan = std::clamp(-static_cast<float>(bestError) / static_cast<float>(kPanFullAngle), -1.0f, 1.0f);
    if (gIsMirrorMode != 0) {
        pan = -pan; // mirror-mode tracks flip left/right
    }
    // Louder as the banana gets closer (position info, so it ignores the steering invert).
    const float t = std::clamp(bestDist / range, 0.0f, 1.0f);
    const float volume = kVolumeNear + t * (kVolumeFar - kVolumeNear);
    // Lower the pitch once the banana falls behind the kart (forward < 0): a Doppler cue.
    const float forward = std::cos(static_cast<float>(bestError) * kS16ToRad);
    const float pitch = 1.0f + kDopplerDrop * std::min(0.0f, forward);

    const int intervalMs = std::clamp(
        CVarGetInteger(CVAR_ACCESS_BANANA_INTERVAL, CVAR_ACCESS_BANANA_INTERVAL_DEFAULT), 100, 3000);
    const int intervalTicks = std::max(1, (intervalMs * kGameTicksPerSecond) / 1000);
    if (mBlipTimer <= 0) {
        AudioCueService::Instance().PlayBananaBeacon(pan, volume, pitch);
        mBlipTimer = intervalTicks;
    } else {
        --mBlipTimer;
    }
}
