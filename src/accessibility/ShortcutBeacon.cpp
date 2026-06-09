#include "ShortcutBeacon.h"

#include "AudioCueService.h"
#include "ScreenReaderService.h"
#include "AccessibilityStrings.h"
#include "AccessibilityCVars.h"

#include <libultraship.h>

#include "engine/tracks/Track.h" // Properties - PathTable2, the per-track raw waypoint paths

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

extern "C" {
#include <common_structs.h> // Player
#include <waypoints.h>      // gTrackPaths, gPathCountByPathIndex, gNearestPathPointByPlayerId
}

extern "C" {
extern Player* gPlayerOne;
// Heading from point a to point b: atan2s(b.x-a.x, b.z-a.z). Negate it to match the
// kart/path heading convention rotation[1] uses (racing/math_util.c).
s32 get_angle_between_two_vectors(f32* a, f32* b);
// Nonzero on mirror-mode tracks, where left and right are flipped (code_800029B0.h).
extern s32 gIsMirrorMode;
// Track section id of the collision triangle the kart is on (collision.c). The shortcut
// path's waypoints carry these same section ids, which is how we know the kart is riding it.
s16 get_track_section_id(u16 index);
// Per-track props of the loaded track (port/Game.h, declared inside its extern "C" block).
Properties* CM_GetProps();
bool IsWarioStadium();
}

using namespace AccessibilityStrings;

namespace {

// The human player is always player 0; the path globals are indexed by player id.
constexpr int kPlayerId = 0;

// s16 binary angles: 0x10000 == 360 degrees. ~45 deg off-heading -> full lean to that side.
constexpr int kPanFullAngle = 0x2000;

// The guidance beep speeds up (longer interval far -> shorter near) and rises in pitch as
// progress is made. Same audio language as the settings-menu demo.
constexpr int kBeepIntervalFar = 26;
constexpr int kBeepIntervalNear = 4;
constexpr int kHitInterval = 9; // chord cadence on a spot / through the exit
constexpr float kPitchFar = 0.9f;
constexpr float kPitchNear = 1.8f;

// Route derivation: shortcut-path points at least this far (XZ) from every main-path point are
// "split off", and a split span needs this many points to count as a shortcut (filters jitter).
constexpr float kSplitDist = 60.0f;
constexpr int kMinRoutePoints = 8;
constexpr int kMaxRawPoints = 3000; // raw-path parse cap, same as the game's own loader

// Approach: the spoken heads-up comes once the fork is at most this many main-path waypoints
// ahead of the kart's progress (~2.5 s) - close enough that "shortcut ahead, right" is acted
// on at the chord, not by turning off the road early. The grace covers a kart already
// slightly past the fork (its main-path progress keeps counting while it peels off).
constexpr int kApproachLeadPoints = 25;
constexpr int kApproachGracePoints = 25;
constexpr float kApproachRange = 1200.0f;
// The spoken heads-up only names a side when the entrance clearly leans to one (|pan| above
// this); near dead-ahead it stays "Shortcut ahead".
constexpr float kSpeakSidePan = 0.12f;

// Re-arm: a consumed heads-up / latch arms again once its fork lies at least this many
// main-path waypoints ahead of the kart (and less than half a lap, so "just passed" does not
// count). Strictly beyond the announce window, so re-arming and announcing can never chase
// each other in a loop. A Lakitu drop-back closer than this is caught separately, by the
// kart's lap progress warping backwards.
constexpr int kRearmMinPoints = kApproachLeadPoints + 15;
constexpr int kWarpBackPoints = 20; // progress rewinding this much in one frame = picked up
// Never speak the heads-up more often than this (~8 s), whatever re-arms it.
constexpr int kSpeakCooldownTicks = 240;

// Latch / release. The guidance latches right at the entrance (within kLatchRange of the
// route's leading stretch) - that is what the single chord marks. It releases quietly when the
// kart clearly chose the main road: only after a short grace, only once the shortcut line is
// genuinely being left (further than kReleaseMinDist - at the fork itself the two lines are
// only ~60 apart, so releasing there would kill the guidance the moment it latched), and the
// kart reads closer to the main line by a margin (or drifts kLoseRange off everything).
constexpr float kLatchRange = 150.0f;
constexpr float kReleaseMinDist = 120.0f;
constexpr float kLoseRange = 300.0f;
constexpr float kMainBiasMargin = 60.0f;
constexpr int kLatchGraceTicks = 45; // ~1.5 s for the kart to start peeling off after the chord

// Carrot-following while latched: the steering guide and the beep aim at the route point this
// many waypoints (~200 units) ahead of the kart, so the target always stays in front and can
// never snap behind. Reaching the last few route points counts as "made it through".
constexpr int kCarrotLeadPoints = 10;
constexpr int kExitTailPoints = 5;
constexpr int kExitChordCount = 3;

// Wario Stadium wall jump. The launch spot is main-path waypoint 0x398, ON the low road right
// where the lap's overpass crosses ~150 units above it (verified against the course data: the
// overpass stretch around main index ~1144 passes 34 units away in plan, ~230 waypoints ahead
// in lap progress). Riding the banked wall there launches the kart up onto the overpass,
// skipping that stretch of lap. The landing index is derived from the data at load: the
// main-path point nearest the launch in space but far from it in progress. Guidance beeps
// toward the launch while it lies a short stretch ahead, rings the chord cadence right on it
// ("climb now"), and the made-it chords ring when the lap progress leaps onto the overpass.
constexpr int kWarioJumpPoint = 0x398;
constexpr float kWarioRange = 900.0f, kWarioReach = 180.0f;
constexpr int kWarioAheadWindow = 120;   // guide while the launch is at most this many points ahead
constexpr int kWarioLeapMinPoints = 120; // progress leaping at least this much in one frame...
constexpr int kWarioLandNearPoints = 60; // ...onto a point this close to the landing = made it

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float DistXZ(float ax, float az, float bx, float bz) {
    const float dx = bx - ax;
    const float dz = bz - az;
    return std::sqrt(dx * dx + dz * dz);
}

// Stereo lean from the kart toward a world point. Convention matches the rest of the drive
// assist: a target to the RIGHT is a negative signed angle, positive pan = right ear.
float PanToward(const Player* player, float tx, float ty, float tz) {
    f32 self[3] = { player->pos[0], player->pos[1], player->pos[2] };
    f32 tgt[3] = { tx, ty, tz };
    const int16_t bearing = static_cast<int16_t>(-get_angle_between_two_vectors(self, tgt));
    const int16_t error = static_cast<int16_t>(bearing - player->rotation[1]);
    float pan = std::clamp(-static_cast<float>(error) / static_cast<float>(kPanFullAngle), -1.0f, 1.0f);
    if (gIsMirrorMode != 0) {
        pan = -pan; // mirror-mode tracks flip left/right
    }
    return pan;
}

// How many waypoints ahead `to` lies from `from` on the circular main path.
int ForwardDelta(int from, int to, int count) {
    return ((to - from) % count + count) % count;
}

} // namespace

void ShortcutBeacon::Reset() {
    mTimer = 0;
    mFollowRoute = -1;
    mExitChords = 0;
    mLatchGrace = 0;
    mSteerActive = false;
    // Re-arm everything: Reset() runs through every race start and the pause menu, so a
    // retried race guides again from the top.
    std::fill(mAnnounceArmed.begin(), mAnnounceArmed.end(), 1);
    std::fill(mLatchArmed.begin(), mLatchArmed.end(), 1);
    mWarioAnnounceArmed = true;
    mWarioPrevMain = -1;
    mPrevMain = -1;
    mSpeakCooldown = 0;
    AudioCueService::Instance().StopShortcutCue();
}

bool ShortcutBeacon::TickExitChords() {
    if (mExitChords <= 0) {
        return false;
    }
    if (mTimer <= 0) {
        AudioCueService::Instance().PlayShortcutHit(0.0f);
        mTimer = kHitInterval;
        --mExitChords;
    } else {
        --mTimer;
    }
    return true;
}

bool ShortcutBeacon::SteerTarget(float out[3]) const {
    if (!mSteerActive) {
        return false;
    }
    out[0] = mSteerTarget[0];
    out[1] = mSteerTarget[1];
    out[2] = mSteerTarget[2];
    return true;
}

void ShortcutBeacon::Tick(ScreenReaderService& reader) {
    if (CVarGetInteger(CVAR_ACCESS_SHORTCUT_CUE, CVAR_ACCESS_SHORTCUT_CUE_DEFAULT) == 0) {
        Reset();
        return;
    }
    if (gPlayerOne == nullptr) {
        Reset();
        return;
    }

    if (mSpeakCooldown > 0) {
        --mSpeakCooldown;
    }

    // Re-derive the routes when the loaded track data changes (track load, mirror toggle).
    const void* path = CM_GetProps()->PathTable2[1];
    if (path != mBuiltPath || gIsMirrorMode != mBuiltMirror ||
        static_cast<int>(gPathCountByPathIndex[0]) != mBuiltMainCount) {
        mBuiltPath = path;
        mBuiltMirror = gIsMirrorMode;
        mBuiltMainCount = gPathCountByPathIndex[0];
        RebuildRoutes();
        mFollowRoute = -1;
        mExitChords = 0;
        mWarioLandIdx = -1;
    }

    if (!mRoutes.empty()) {
        if (!TickRoutes(reader)) {
            AudioCueService::Instance().StopShortcutCue(); // idle: no route applies right now
            mTimer = 0;
        }
        return;
    }
    if (IsWarioStadium()) {
        TickWarioJump(reader);
        return;
    }
    Reset();
}

void ShortcutBeacon::RebuildRoutes() {
    mRoutes.clear();

    // A second progression path the game itself never loads (its runtime size slot stays < 2)
    // is unused data: on the stock tracks only Koopa Troopa Beach has one, and it is the
    // shortcut lap. Tracks that DO load their extra paths (Yoshi Valley, D_80163368[1] >= 2)
    // are true route splits, not shortcuts - skip those.
    const TrackPathPoint* raw = CM_GetProps()->PathTable2[1];
    const TrackPathPoint* mainPath = gTrackPaths[0];
    const int mainCount = gPathCountByPathIndex[0];
    if (raw == nullptr || mainPath == nullptr || mainCount < 2 || D_80163368[1] >= 2) {
        mAnnounceArmed.clear();
        mLatchArmed.clear();
        return;
    }

    // Parse the raw, sentinel-terminated path, mirroring X exactly like the game's own
    // process_path_data so it lands in the same space as gTrackPaths.
    std::vector<RoutePoint> pts;
    for (int i = 0; i < kMaxRawPoints; ++i) {
        const TrackPathPoint& p = raw[i];
        if ((p.x & 0xFFFF) == 0x8000 && (p.y & 0xFFFF) == 0x8000 && (p.z & 0xFFFF) == 0x8000) {
            break;
        }
        const float x = (gIsMirrorMode != 0) ? -static_cast<float>(p.x) : static_cast<float>(p.x);
        pts.push_back({ x, static_cast<float>(p.y), static_cast<float>(p.z), p.trackSectionId });
    }

    // Sections the main route drives, so we can spot the sections only the shortcut touches.
    // get_track_section_id masks to 8 bits; 0xFF marks untagged boundary geometry.
    bool mainSecs[256] = {};
    for (int i = 0; i < mainCount; ++i) {
        mainSecs[mainPath[i].trackSectionId & 0xFF] = true;
    }

    // Walk the shortcut path; each span that pulls away from every main-path point is a route.
    int runStart = -1;
    for (int i = 0; i <= static_cast<int>(pts.size()); ++i) {
        bool split = false;
        if (i < static_cast<int>(pts.size())) {
            float best = 1e18f;
            for (int j = 0; j < mainCount; ++j) {
                const float dx = static_cast<float>(mainPath[j].x) - pts[i].x;
                const float dz = static_cast<float>(mainPath[j].z) - pts[i].z;
                best = std::min(best, dx * dx + dz * dz);
            }
            split = best > kSplitDist * kSplitDist;
        }
        if (split) {
            if (runStart < 0) {
                runStart = i;
            }
            continue;
        }
        if (runStart < 0) {
            continue;
        }
        if (i - runStart >= kMinRoutePoints) {
            Route route;
            route.points.assign(pts.begin() + runStart, pts.begin() + i);
            route.mouthIdx = -1;
            for (int j = 0; j < static_cast<int>(route.points.size()); ++j) {
                const uint16_t sec = route.points[j].sectionId & 0xFF;
                if (sec == 0xFF || mainSecs[sec]) {
                    continue;
                }
                if (route.mouthIdx < 0) {
                    route.mouthIdx = j;
                }
                if (std::find(route.shortcutSecs.begin(), route.shortcutSecs.end(), sec) ==
                    route.shortcutSecs.end()) {
                    route.shortcutSecs.push_back(sec);
                }
            }
            // Nearest main-path waypoint to the split's first point = where the routes fork.
            float best = 1e18f;
            route.approachMainIdx = 0;
            for (int j = 0; j < mainCount; ++j) {
                const float dx = static_cast<float>(mainPath[j].x) - route.points[0].x;
                const float dz = static_cast<float>(mainPath[j].z) - route.points[0].z;
                const float d = dx * dx + dz * dz;
                if (d < best) {
                    best = d;
                    route.approachMainIdx = j;
                }
            }
            if (route.mouthIdx >= 0 && !route.shortcutSecs.empty()) {
                mRoutes.push_back(std::move(route));
            }
        }
        runStart = -1;
    }

    mAnnounceArmed.assign(mRoutes.size(), 1);
    mLatchArmed.assign(mRoutes.size(), 1);
}

bool ShortcutBeacon::TickRoutes(ScreenReaderService& reader) {
    const Player* player = gPlayerOne;
    const float px = player->pos[0];
    const float pz = player->pos[2];
    const uint16_t playerSec = get_track_section_id(player->collision.meshIndexZX) & 0xFF;
    const int mainCount = gPathCountByPathIndex[0];
    const TrackPathPoint* mainPath = gTrackPaths[0];
    const int playerMain = gNearestPathPointByPlayerId[kPlayerId];
    mSteerActive = false; // republished below while actually leading along a route

    // Re-arm: whenever a route's fork lies clearly ahead of the kart again (next lap, retried
    // race) - strictly beyond the announce window, so re-arm and announce can never loop - or
    // when the kart's lap progress warps backwards (Lakitu fishing it out of the water and
    // dropping it behind the entrance), so the shortcut can always be re-attempted.
    const int prevMain = mPrevMain;
    mPrevMain = playerMain;
    const int warpBack = (prevMain >= 0) ? ForwardDelta(playerMain, prevMain, mainCount) : 0;
    const bool warpedBack = warpBack >= kWarpBackPoints && warpBack < mainCount / 2;
    for (int k = 0; k < static_cast<int>(mRoutes.size()); ++k) {
        if (mFollowRoute == k) {
            continue;
        }
        const int ahead = ForwardDelta(playerMain, mRoutes[k].approachMainIdx, mainCount);
        if (warpedBack || (ahead > kRearmMinPoints && ahead < mainCount / 2)) {
            mAnnounceArmed[k] = 1;
            mLatchArmed[k] = 1;
        }
    }

    // Nearest route point to the kart, returned via nearest; the distance is the result.
    auto nearestOn = [&](const Route& route, int& nearest) {
        nearest = 0;
        float best = 1e18f;
        for (int j = 0; j < static_cast<int>(route.points.size()); ++j) {
            const float d = DistXZ(px, pz, route.points[j].x, route.points[j].z);
            if (d < best) {
                best = d;
                nearest = j;
            }
        }
        return best;
    };
    // Distance from the kart to the main racing line (nearest main-path waypoint).
    auto distToMain = [&]() {
        float best = 1e18f;
        for (int j = 0; j < mainCount; ++j) {
            best = std::min(best, DistXZ(px, pz, static_cast<float>(mainPath[j].x),
                                         static_cast<float>(mainPath[j].z)));
        }
        return best;
    };

    auto onShortcutGround = [&](const Route& route) {
        return std::find(route.shortcutSecs.begin(), route.shortcutSecs.end(), playerSec) !=
               route.shortcutSecs.end();
    };

    // Latch onto a route: by collision section anywhere along it (definitely riding it), or by
    // reaching its leading stretch (up to the mouth - the part that sits on the racing line at
    // the fork). The proximity latch is tight, so the single chord marks the entrance itself,
    // and fires at most once per lap per route so it cannot ring over and over.
    if (mFollowRoute < 0) {
        for (int k = 0; k < static_cast<int>(mRoutes.size()) && mFollowRoute < 0; ++k) {
            const Route& route = mRoutes[k];
            bool latch = onShortcutGround(route);
            if (!latch && mLatchArmed[k] != 0) {
                const int ahead = ForwardDelta(playerMain, route.approachMainIdx, mainCount);
                if (ahead <= kApproachLeadPoints || ahead >= mainCount - kApproachGracePoints) {
                    int nearest = 0;
                    latch = nearestOn(route, nearest) <= kLatchRange && nearest <= route.mouthIdx;
                }
            }
            if (latch) {
                // Entrance reached: ring the chord once - "shortcut guidance engaged".
                mFollowRoute = k;
                mLatchArmed[k] = 0;
                mLatchGrace = kLatchGraceTicks;
                AudioCueService::Instance().PlayShortcutHit(0.0f);
                mTimer = kHitInterval;
            }
        }
    }

    // Following: the carrot (a point a little ahead on the route) goes to the steering guide,
    // so the engine pan leads the kart along the shortcut's own racing line, and the beep
    // ticks along in agreement, rising with progress. Staying on the main road instead reads
    // as "closer to the main line than to the shortcut" and lets go quietly for this lap.
    if (mFollowRoute >= 0) {
        const Route& route = mRoutes[mFollowRoute];
        const int last = static_cast<int>(route.points.size()) - 1;
        int nearest = 0;
        const float dist = nearestOn(route, nearest);
        const bool riding = onShortcutGround(route);
        if (mLatchGrace > 0) {
            --mLatchGrace;
        }
        // Release checks: never during the post-latch grace and never while the shortcut line
        // is still close (at the fork the two lines nearly touch). "Chose the main road" stays
        // consumed; being yanked away (fell in the water, Lakitu) re-arms the latch so the
        // entrance can be re-attempted right away.
        const bool releaseWindow = mLatchGrace <= 0 && dist > kReleaseMinDist;
        const bool mainBias = releaseWindow && distToMain() + kMainBiasMargin < dist;
        const bool yankedAway = releaseWindow && dist > kLoseRange && !mainBias;
        if (nearest >= last - kExitTailPoints + 1) {
            // Reached the far end. Chords only if the kart actually came through on/near the
            // route - never for a kart that just converged with it along the main road.
            if (riding || dist <= kLatchRange) {
                mExitChords = kExitChordCount;
                mTimer = 0;
            }
            mFollowRoute = -1;
        } else if (!riding && (mainBias || yankedAway)) {
            if (yankedAway) {
                mLatchArmed[mFollowRoute] = 1;
            }
            mFollowRoute = -1;
        } else {
            const RoutePoint& tgt = route.points[std::min(nearest + kCarrotLeadPoints, last)];
            mSteerTarget[0] = tgt.x;
            mSteerTarget[1] = tgt.y;
            mSteerTarget[2] = tgt.z;
            mSteerActive = true;
            const float p = static_cast<float>(nearest) / static_cast<float>(last);
            const int interval = std::max(1, static_cast<int>(Lerp(kBeepIntervalFar, kBeepIntervalNear, p)));
            const float pitch = Lerp(kPitchFar, kPitchNear, p);
            if (mTimer <= 0) {
                AudioCueService::Instance().PlayShortcutBeep(pitch, PanToward(player, tgt.x, tgt.y, tgt.z));
                mTimer = interval;
            } else {
                --mTimer;
            }
            return true;
        }
    }

    if (TickExitChords()) {
        return true;
    }

    // Approach: the fork lies a little ahead of the kart's main-path progress. Speak the
    // heads-up once per lap with the side the shortcut leaves on - that is all: the player
    // keeps driving the road as normal (the engine pan still follows the main line here, and
    // the shortcut entrance is ON the road), and the chord above marks the moment to follow
    // the pan into the shortcut. No approach beeps: a side called early tempted players to
    // turn off the road - into the sea on Koopa - long before the entrance.
    for (int k = 0; k < static_cast<int>(mRoutes.size()); ++k) {
        const Route& route = mRoutes[k];
        if (mLatchArmed[k] == 0 || mAnnounceArmed[k] == 0 || mSpeakCooldown > 0) {
            continue;
        }
        const int ahead = ForwardDelta(playerMain, route.approachMainIdx, mainCount);
        if (ahead > kApproachLeadPoints && ahead < mainCount - kApproachGracePoints) {
            continue;
        }
        const RoutePoint& fork = route.points[0];
        if (DistXZ(px, pz, fork.x, fork.z) > kApproachRange) {
            continue;
        }
        // The side is judged from the kart's own heading toward the mouth (the first clearly
        // off-road point), with the same tested pan convention as every other cue.
        mAnnounceArmed[k] = 0;
        mSpeakCooldown = kSpeakCooldownTicks;
        const RoutePoint& mouth = route.points[route.mouthIdx];
        const float sidePan = PanToward(player, mouth.x, mouth.y, mouth.z);
        std::string phrase = SHORTCUT_AHEAD;
        if (sidePan <= -kSpeakSidePan) {
            phrase += SHORTCUT_SIDE_LEFT;
        } else if (sidePan >= kSpeakSidePan) {
            phrase += SHORTCUT_SIDE_RIGHT;
        }
        reader.Speak(phrase, false);
        break;
    }
    return false;
}

void ShortcutBeacon::TickWarioJump(ScreenReaderService& reader) {
    const Player* player = gPlayerOne;
    const int count = gPathCountByPathIndex[0];
    const TrackPathPoint* mainPath = gTrackPaths[0];
    if (mainPath == nullptr || count <= kWarioJumpPoint) {
        Reset();
        return;
    }
    // Derive the landing once per load: the main-path point nearest the launch in space but
    // far from it in lap progress = the overpass crossing right above it. The live path is
    // already mirrored by the game's loader, so mirror mode works out by itself.
    if (mWarioLandIdx < 0) {
        const TrackPathPoint& lp = mainPath[kWarioJumpPoint];
        float best = 1e18f;
        for (int j = 0; j < count; ++j) {
            const int apart = std::min(ForwardDelta(j, kWarioJumpPoint, count),
                                       ForwardDelta(kWarioJumpPoint, j, count));
            if (apart <= kWarioAheadWindow) {
                continue;
            }
            const float d = DistXZ(lp.x, lp.z, mainPath[j].x, mainPath[j].z);
            if (d < best) {
                best = d;
                mWarioLandIdx = j;
            }
        }
        if (mWarioLandIdx < 0) {
            Reset();
            return;
        }
    }
    const TrackPathPoint& launch = mainPath[kWarioJumpPoint];
    const TrackPathPoint& landing = mainPath[mWarioLandIdx];
    const int playerMain = gNearestPathPointByPlayerId[kPlayerId];
    const int prevMain = mWarioPrevMain;
    mWarioPrevMain = playerMain;

    // Making the climb = the kart's lap progress leaps forward onto the overpass stretch in
    // one frame. Ring the made-it chords. (A Lakitu drop-back is a backwards warp and a race
    // restart lands far from the landing stretch, so neither qualifies.)
    if (prevMain >= 0) {
        const int leap = ForwardDelta(prevMain, playerMain, count);
        const int landed = std::min(ForwardDelta(playerMain, mWarioLandIdx, count),
                                    ForwardDelta(mWarioLandIdx, playerMain, count));
        if (leap >= kWarioLeapMinPoints && leap <= count / 2 && landed <= kWarioLandNearPoints) {
            mExitChords = kExitChordCount;
            mTimer = 0;
        }
    }
    if (TickExitChords()) {
        return;
    }

    // Re-arm the heads-up once the launch lies comfortably ahead again (next lap or a retry).
    const int ahead = ForwardDelta(playerMain, kWarioJumpPoint, count);
    if (ahead > kRearmMinPoints && ahead < count / 2) {
        mWarioAnnounceArmed = true;
    }
    // Guide only while the launch spot lies a short stretch ahead on the road; past it (or
    // anywhere else in the lap, including driving the overpass above) stay quiet.
    const float dist = DistXZ(player->pos[0], player->pos[2], launch.x, launch.z);
    if (ahead > kWarioAheadWindow || dist > kWarioRange) {
        AudioCueService::Instance().StopShortcutCue();
        mTimer = 0;
        return;
    }

    // Approaching the launch: speak the heads-up once, toward the side the overpass leans.
    if (mWarioAnnounceArmed && ahead <= kApproachLeadPoints && mSpeakCooldown <= 0) {
        mWarioAnnounceArmed = false;
        mSpeakCooldown = kSpeakCooldownTicks;
        const float sidePan = PanToward(player, landing.x, landing.y, landing.z);
        std::string phrase = SHORTCUT_AHEAD;
        if (sidePan <= -kSpeakSidePan) {
            phrase += SHORTCUT_SIDE_LEFT;
        } else if (sidePan >= kSpeakSidePan) {
            phrase += SHORTCUT_SIDE_RIGHT;
        }
        reader.Speak(phrase, false);
    }

    const float pan = PanToward(player, launch.x, launch.y, launch.z);
    if (dist <= kWarioReach) {
        // On the launch spot, under the overpass: the chord cadence says "climb the wall now".
        if (mTimer <= 0) {
            AudioCueService::Instance().PlayShortcutHit(pan);
            mTimer = kHitInterval;
        } else {
            --mTimer;
        }
        return;
    }

    const float span = std::max(1.0f, kWarioRange - kWarioReach);
    const float p = std::clamp((kWarioRange - dist) / span, 0.0f, 1.0f);
    const int interval = std::max(1, static_cast<int>(Lerp(kBeepIntervalFar, kBeepIntervalNear, p)));
    const float pitch = Lerp(kPitchFar, kPitchNear, p);
    if (mTimer <= 0) {
        AudioCueService::Instance().PlayShortcutBeep(pitch, pan);
        mTimer = interval;
    } else {
        --mTimer;
    }
}
