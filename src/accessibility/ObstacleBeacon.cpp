#include "ObstacleBeacon.h"

#include "AudioCueService.h"
#include "AccessibilityCVars.h"

#include <libultraship.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

extern "C" {
#include <common_structs.h> // Player
#include <actor_types.h>    // struct Actor, enum ActorType (ACTOR_CAR, ACTOR_FALLING_ROCK...)
}

extern "C" {
extern Player* gPlayerOne;
// Actor list accessors (port/Game.cpp). Obstacles live in this list as generic Actors;
// the fields we read (type@0x00, flags@0x02, boundingBoxSize@0x0C, pos@0x18) are part of
// the base Actor layout, so no specialized struct is needed.
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

// A parked falling rock sits at its spawn point (velocity 0) while waiting to drop again,
// keeping its actor allocated the whole time; only warn once it is actually moving (falling
// or bouncing). Squared speed threshold, so a still rock (exactly 0) is skipped but a rock
// under gravity (velocity grows past 0.1/frame immediately) is caught.
constexpr float kRockMovingSpeedSq = 0.001f;

// Warning distance (world units) chosen by the Obstacle range slider. Tighter than the
// item-box / banana beacons - this is a "you are about to hit it" cue, not a long-range
// pickup guide. Kart top speed is ~9 units/frame, so this gives roughly a quarter second
// (close) to a second-ish (far) of reaction time, and a bit more for oncoming traffic.
constexpr float kRangeMin = 60.0f;  // slider 0: only warns when nearly on top of it
constexpr float kRangeMax = 300.0f; // slider 100: warns from further out

constexpr float kVolumeNear = 1.00f; // hazard right on top of you (a crash cue must cut over the engines)
constexpr float kVolumeFar = 0.60f;  // at the edge of the warning range (kept loud - the radar must stand out)

// Same Doppler "it is behind you" pitch drop as the other beacons: forward = cos(angle to
// the hazard) is +1 dead ahead, 0 abeam, -1 directly behind; the pitch is left at 1.0 while
// ahead and lowered only once the hazard falls behind (you have passed it).
constexpr float kDopplerDrop = 0.30f; // pitch at directly behind = 1.0 - this (= 0.70)

// The blip repeats on the Obstacle loop-time slider (milliseconds); the beacon is ticked
// once per ~30 fps game frame, so ms is converted to ticks. Smaller = faster / more urgent.
constexpr int kGameTicksPerSecond = 30;

// Collision-course test (closest point of approach). A hazard only warns if its predicted
// closest pass to the kart, on their current headings, is within this distance - a head-on or
// slightly diagonal hit - so a car you will slip past on the side stays silent. Added on top
// of the two bounding-box radii. Larger = more lenient (warns for wider near-misses).
constexpr float kHeadOnMargin = 10.0f;
// Minimum relative speed (squared, units/frame) for the CPA test to mean anything: below this
// the kart and obstacle are essentially co-moving (no closing), so no collision is predicted.
constexpr float kMinRelSpeedSq = 0.25f;

// Extra warning distance granted per unit of closing speed (in game frames of look-ahead). A
// head-on approach closes fast, so a fixed distance gives almost no time to react; extending
// the range by closingSpeed * this many frames gives roughly constant lead TIME to dodge,
// while a slow / stationary hazard keeps the tight slider range. ~30 frames ≈ a second of
// closing added on top of the static range, so an oncoming car is heard well before contact
// (raise it for even more lead; the CPA gate still blocks side-passes so it will not spam).
constexpr float kClosingLeadFrames = 30.0f;

// Dynamic obstacles a player can crash into and should be warned about. Trees / signs /
// cacti / bushes and the item/shell/banana actors are deliberately excluded (scenery or
// pickups, handled by their own cues).
bool IsObstacle(int16_t type) {
    switch (type) {
        case ACTOR_FALLING_ROCK:
        case ACTOR_PIRANHA_PLANT:
        case ACTOR_TRAIN_ENGINE:
        case ACTOR_TRAIN_TENDER:
        case ACTOR_TRAIN_PASSENGER_CAR:
        case ACTOR_COW:
        case ACTOR_BOX_TRUCK:
        case ACTOR_PADDLE_BOAT:
        case ACTOR_SCHOOL_BUS:
        case ACTOR_TANKER_TRUCK:
        case ACTOR_CAR:
            return true;
        default:
            return false;
    }
}

} // namespace

void ObstacleBeacon::Reset() {
    mBlipTimer = 0;
    AudioCueService::Instance().StopObstacleBeacon();
}

void ObstacleBeacon::Tick() {
    if (CVarGetInteger(CVAR_ACCESS_OBSTACLE_CUE, CVAR_ACCESS_OBSTACLE_CUE_DEFAULT) == 0) {
        Reset();
        return;
    }
    const Player* player = gPlayerOne;
    if (player == nullptr) {
        Reset();
        return;
    }

    const float sens = std::clamp(
        CVarGetInteger(CVAR_ACCESS_OBSTACLE_RANGE, CVAR_ACCESS_OBSTACLE_RANGE_DEFAULT) / 100.0f, 0.0f, 1.0f);
    const float range = kRangeMin + sens * (kRangeMax - kRangeMin);

    const float px = player->pos[0];
    const float pz = player->pos[2];
    const int16_t heading = player->rotation[1];

    // Pick the nearest in-range hazard you are on a collision course with: within its warning
    // zone AND closing on it (head-on, a diagonal intercept, or catching a slower one ahead).
    // A hazard you are driving away from or have already passed is ignored, so the cue only
    // fires when a crash is actually coming. Bigger hazards (larger hitbox) warn a touch sooner.
    float bestDist = 0.0f;
    float bestDanger = 0.0f;
    int16_t bestError = 0;
    bool found = false;
    const size_t count = CM_GetActorSize();
    for (size_t i = 0; i < count; ++i) {
        struct Actor* actor = CM_GetActor(i);
        if (actor == nullptr || actor->flags == 0) {
            continue; // skip empty / removed slots (same active-actor gate as the other beacons)
        }
        if (!IsObstacle(actor->type)) {
            continue;
        }
        // Skip a falling rock that is parked at its spawn point waiting to drop (velocity 0):
        // only an actually-falling rock is an imminent hazard.
        if (actor->type == ACTOR_FALLING_ROCK) {
            const float vx = actor->velocity[0];
            const float vy = actor->velocity[1];
            const float vz = actor->velocity[2];
            if ((vx * vx + vy * vy + vz * vz) < kRockMovingSpeedSq) {
                continue;
            }
        }
        const float dx = actor->pos[0] - px;
        const float dz = actor->pos[2] - pz;
        const float dist = std::sqrt(dx * dx + dz * dz);

        // Relative velocity of the obstacle with respect to the kart (used for both the
        // closing-speed lead and the collision-course prediction below).
        const float rvx = actor->velocity[0] - player->velocity[0];
        const float rvz = actor->velocity[2] - player->velocity[2];
        const float rvv = rvx * rvx + rvz * rvz;

        // Closing speed (units/frame, positive = gap shrinking). A fast head-on approach
        // earns extra warning distance so it is announced with enough lead time to dodge.
        float closingSpeed = 0.0f;
        if (dist > 0.001f) {
            closingSpeed = -(dx * rvx + dz * rvz) / dist;
        }
        const float danger =
            range + actor->boundingBoxSize + std::max(0.0f, closingSpeed) * kClosingLeadFrames;
        if (dist >= danger) {
            continue; // outside this hazard's warning zone
        }
        // Collision-course gate (closest point of approach): on their current headings, will the
        // kart and the obstacle actually pass within collision distance - a head-on / slightly
        // diagonal hit - or slip past on the side? Predict the closest approach and require it to
        // fall within the two bounding boxes plus a small diagonal tolerance.
        const float hitDist = player->boundingBoxSize + actor->boundingBoxSize + kHeadOnMargin;
        if (dist > hitDist) { // not already in contact range: predict the pass
            if (rvv < kMinRelSpeedSq) {
                continue; // co-moving / no closing -> no collision coming
            }
            const float tStar = -(dx * rvx + dz * rvz) / rvv; // time of closest approach (frames)
            if (tStar < 0.0f) {
                continue; // closest approach already passed -> moving apart
            }
            const float cx = dx + rvx * tStar; // separation vector at closest approach
            const float cz = dz + rvz * tStar;
            if ((cx * cx + cz * cz) > (hitDist * hitDist)) {
                continue; // will slip past on the side, not a frontal collision
            }
        }
        // Closest hazard wins (most imminent crash). Compare the raw gap.
        if (found && dist >= bestDist) {
            continue;
        }
        f32 self[3] = { px, player->pos[1], pz };
        f32 obstaclePos[3] = { actor->pos[0], actor->pos[1], actor->pos[2] };
        const int16_t bearing = static_cast<int16_t>(-get_angle_between_two_vectors(self, obstaclePos));
        bestError = static_cast<int16_t>(bearing - heading);
        bestDist = dist;
        bestDanger = danger;
        found = true;
    }

    if (!found) {
        Reset();
        return;
    }

    // Pan toward the hazard so you know which side it is on and can steer away. The obstacle
    // cue panned to the opposite side of the hazard in play, so its sign is flipped relative
    // to the other beacons (verified by ear): a hazard on the RIGHT now leans the cue right.
    float pan = std::clamp(static_cast<float>(bestError) / static_cast<float>(kPanFullAngle), -1.0f, 1.0f);
    if (gIsMirrorMode != 0) {
        pan = -pan; // mirror-mode tracks flip left/right
    }
    // Louder as the hazard gets closer (position info, so it ignores the steering invert).
    const float t = std::clamp(bestDist / bestDanger, 0.0f, 1.0f);
    const float volume = kVolumeNear + t * (kVolumeFar - kVolumeNear);
    // Lower the pitch once the hazard falls behind the kart (forward < 0): a Doppler cue.
    const float forward = std::cos(static_cast<float>(bestError) * kS16ToRad);
    const float pitch = 1.0f + kDopplerDrop * std::min(0.0f, forward);

    const int intervalMs = std::clamp(
        CVarGetInteger(CVAR_ACCESS_OBSTACLE_INTERVAL, CVAR_ACCESS_OBSTACLE_INTERVAL_DEFAULT), 30, 1000);
    const int intervalTicks = std::max(1, (intervalMs * kGameTicksPerSecond) / 1000);
    if (mBlipTimer <= 0) {
        AudioCueService::Instance().PlayObstacleBeacon(pan, volume, pitch);
        mBlipTimer = intervalTicks;
    } else {
        --mBlipTimer;
    }
}
