#include "ItemBoxBeacon.h"

#include "AudioCueService.h"
#include "AccessibilityCVars.h"

#include <libultraship.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

extern "C" {
#include <common_structs.h> // Player
#include <actor_types.h>    // struct Actor / struct ItemBox, enum ActorType (ACTOR_ITEM_BOX...)
#include <defines.h>        // ITEM_NONE
}

extern "C" {
extern Player* gPlayerOne;
// Actor list accessors (port/Game.cpp). Item boxes live in this list as ACTOR_ITEM_BOX
// / ACTOR_HOT_AIR_BALLOON_ITEM_BOX entries, laid out as struct ItemBox.
size_t CM_GetActorSize(void);
struct Actor* CM_GetActor(size_t index);
// Heading from point a to point b: atan2s(b.x-a.x, b.z-a.z). Negate it to match the
// kart/path heading convention rotation[1] uses (racing/math_util.c).
s32 get_angle_between_two_vectors(f32* a, f32* b);
// Nonzero on mirror-mode tracks, where left and right are flipped (code_800029B0.h).
extern s32 gIsMirrorMode;
}

namespace {

// An item box is grabbable only in this state (see update_actor_item_box: 0/1 = rising
// in, 2 = active, 3 = respawning after being hit).
constexpr int kItemBoxActiveState = 2;

// Angle units are s16 binary angles (0x10000 == 360 deg, ~182 per degree).
constexpr int kPanFullAngle = 0x2000;                  // ~45 deg off-heading: full lean to that side
constexpr float kS16ToRad = 3.14159265358979f / 32768.0f; // s16 binary angle -> radians (32768 == pi)

// Detection range (world units) chosen by the Item-box range slider. Kart top speed is
// ~9 units/frame (~270 u/s at 30 fps) and the grab distance is ~11 units, so this band
// gives roughly half a second (close) to a couple of seconds (far) of lead time.
constexpr float kRangeMin = 100.0f; // slider 0: only guides you when nearly on top of it
constexpr float kRangeMax = 600.0f; // slider 100: warns from well ahead

constexpr float kVolumeNear = 0.95f; // right on the box (item boxes are an important cue)
constexpr float kVolumeFar = 0.40f;  // at the edge of the detection range

// Doppler "you passed it": the blip keeps sounding once the box is behind you but drops
// in pitch. forward = cos(angle to the box) is +1 dead ahead, 0 abeam, -1 directly
// behind; the pitch is left at 1.0 while ahead and lowered only as it falls behind.
constexpr float kDopplerDrop = 0.30f; // pitch at directly behind = 1.0 - this (= 0.70)

constexpr int kBlipInterval = 18; // ticks between blips (~600 ms at the 30 fps game tick)

} // namespace

void ItemBoxBeacon::Reset() {
    mBlipTimer = 0;
    AudioCueService::Instance().StopItemBoxBeacon();
}

void ItemBoxBeacon::Tick() {
    if (CVarGetInteger(CVAR_ACCESS_ITEMBOX_CUE, CVAR_ACCESS_ITEMBOX_CUE_DEFAULT) == 0) {
        Reset();
        return;
    }
    const Player* player = gPlayerOne;
    if (player == nullptr) {
        Reset();
        return;
    }
    // Only guide toward a box while we can actually pick one up. Grabbing a box gives an
    // item, so a held item naturally silences the beacon ("stops when you collect it").
    if (player->currentItemCopy != ITEM_NONE) {
        Reset();
        return;
    }

    const float sens = std::clamp(
        CVarGetInteger(CVAR_ACCESS_ITEMBOX_RANGE, CVAR_ACCESS_ITEMBOX_RANGE_DEFAULT) / 100.0f, 0.0f, 1.0f);
    const float range = kRangeMin + sens * (kRangeMax - kRangeMin);

    const float px = player->pos[0];
    const float pz = player->pos[2];
    const int16_t heading = player->rotation[1];

    // Pick the nearest active item box within range, in any direction. While it is ahead
    // the blip guides you onto it; once you drive past it the same box keeps sounding from
    // behind at a lower pitch (the Doppler "you passed it" cue) until it leaves range.
    float bestDist = range;
    int16_t bestError = 0;
    bool found = false;
    const size_t count = CM_GetActorSize();
    for (size_t i = 0; i < count; ++i) {
        struct Actor* actor = CM_GetActor(i);
        if (actor == nullptr || actor->flags == 0) {
            continue;
        }
        if (actor->type != ACTOR_ITEM_BOX && actor->type != ACTOR_HOT_AIR_BALLOON_ITEM_BOX) {
            continue;
        }
        const struct ItemBox* box = reinterpret_cast<const struct ItemBox*>(actor);
        if (box->state != kItemBoxActiveState) {
            continue;
        }
        const float dx = box->pos[0] - px;
        const float dz = box->pos[2] - pz;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist >= bestDist) {
            continue;
        }
        f32 self[3] = { px, player->pos[1], pz };
        f32 boxPos[3] = { box->pos[0], box->pos[1], box->pos[2] };
        const int16_t bearing = static_cast<int16_t>(-get_angle_between_two_vectors(self, boxPos));
        const int16_t error = static_cast<int16_t>(bearing - heading);
        bestDist = dist;
        bestError = error;
        found = true;
    }

    if (!found) {
        Reset();
        return;
    }

    // Pan toward the box (drive toward the sound). Convention: a target to the RIGHT is a
    // negative signed angle, and positive pan = right ear (matches the steering guide).
    float pan = std::clamp(-static_cast<float>(bestError) / static_cast<float>(kPanFullAngle), -1.0f, 1.0f);
    if (gIsMirrorMode != 0) {
        pan = -pan; // mirror-mode tracks flip left/right
    }
    // Louder as the box gets closer (position info, so it ignores the steering invert).
    const float t = std::clamp(bestDist / range, 0.0f, 1.0f);
    const float volume = kVolumeNear + t * (kVolumeFar - kVolumeNear);
    // Lower the pitch once the box falls behind the kart (forward < 0), leaving it at the
    // normal pitch while ahead: a Doppler-style "you passed it" cue.
    const float forward = std::cos(static_cast<float>(bestError) * kS16ToRad);
    const float pitch = 1.0f + kDopplerDrop * std::min(0.0f, forward);

    if (mBlipTimer <= 0) {
        AudioCueService::Instance().PlayItemBoxBeacon(pan, volume, pitch);
        mBlipTimer = kBlipInterval;
    } else {
        --mBlipTimer;
    }
}
