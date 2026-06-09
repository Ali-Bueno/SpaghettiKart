#pragma once

#include <cstdint>
#include <vector>

class ScreenReaderService;

/**
 * Audio beacon that guides a blind player along a track's known shortcuts.
 *
 * The shortcut geometry comes from the game's own course data. Every track ships up to four
 * "track_waypoints" progression paths (Track::PathTable2); Koopa Troopa Beach ships a complete
 * second lap path that drives both of its shortcuts (the water cut behind the first big rock
 * and the cave), but the game never loads it - its PathSizes slot is 1, so it is unused data.
 * We parse that raw path ourselves, diff it against the live main path (gTrackPaths[0]), and
 * every span where it pulls away from the main route becomes a guided shortcut. The waypoints
 * carry their own trackSectionIds, so the sections only the shortcut path touches tell us,
 * via the player's current collision section, when the kart is actually riding the shortcut.
 * Tracks whose extra paths ARE loaded by the game (Yoshi Valley's four-way fork) are real
 * route splits, not shortcuts, and are skipped - MultiPathGuide covers those.
 *
 * Wario Stadium's wall-jump shortcut is not a separate path; its launch spot is main-path
 * waypoint 0x398, read from gTrackPaths[0] at runtime so the game's own path mirroring keeps
 * it correct in mirror mode.
 *
 * The guidance, approach by approach (re-armed whenever the fork lies ahead of the kart
 * again - the next lap, a pause-menu retry, a Lakitu drop-back - so it can always be
 * re-attempted):
 *  1. Approach: ~2.5 s before the fork, a spoken heads-up with the side - "Shortcut ahead,
 *     left". Nothing else: the entrance is ON the road, so the player just keeps driving it
 *     (no approach beeps - a side called early tempted players to leave the road long before
 *     the entrance, into the sea on Koopa).
 *  2. Entrance: right at the fork the beacon latches: the chord rings ONCE and the carrot (a
 *     point a few waypoints ahead on the shortcut line - never a fixed spot, so it cannot snap
 *     behind) is handed to the drive assist's steering guide (SteerTarget). From here the
 *     engine pan - the steering language the player already drives by - follows the shortcut's
 *     own racing line all the way through, and the beacon's beep ticks along in agreement,
 *     rising with progress.
 *  3. Exit: reaching the far end rings the chord again (three times) and everything hands back
 *     to the main path. Staying on the main road instead lets go quietly (the kart reads
 *     clearly closer to the main line than to the shortcut - never during the short post-latch
 *     grace, and never while the two lines still run together) and stays quiet until the fork
 *     is ahead again. Falling in the water (a Lakitu yank far off both lines) re-arms the
 *     latch immediately, so the same entrance can be retried within the lap.
 *
 * Audio only - it never changes how the kart drives. Like the rest of the drive assist it only
 * runs during the live race; AccessibilityManager gates that and calls Reset() otherwise.
 */
class ShortcutBeacon {
  public:
    // Silence the beacon and forget any in-progress ride (call when leaving the live race).
    void Reset();
    // Resolve the current track's shortcuts and emit the timed guidance cue. Once per frame.
    void Tick(ScreenReaderService& reader);
    // While leading the kart along a shortcut route, writes the look-ahead point on the route
    // (the carrot) to out[3] and returns true. The drive assist aims its steering guide (the
    // engine pan) at this instead of the main racing line, so the player drives the shortcut
    // with the exact same audio language and the two guides never fight.
    bool SteerTarget(float out[3]) const;

  private:
    struct RoutePoint {
        float x, y, z;
        uint16_t sectionId;
    };
    // One span where the unused shortcut path pulls away from the main route (= one shortcut).
    struct Route {
        std::vector<RoutePoint> points;     // shortcut-path waypoints across the span
        std::vector<uint16_t> shortcutSecs; // track sections only the shortcut touches
        int mouthIdx;        // first point on a shortcut-only section (end of the lead-in part)
        int approachMainIdx; // main-path waypoint index where the routes begin to split
    };

    // Re-derive the routes from the loaded track's path data (on track load / mirror change).
    void RebuildRoutes();
    // Guidance along the data-driven routes; false if nothing applies this frame.
    bool TickRoutes(ScreenReaderService& reader);
    // Wall-jump guidance for Wario Stadium (announce, beep at the wall, chords on landing).
    void TickWarioJump(ScreenReaderService& reader);
    // Plays any pending "made it through" chords; true while they are still sounding.
    bool TickExitChords();

    std::vector<Route> mRoutes;
    // Per route: whether the heads-up / the proximity latch may fire. Consumed when they do,
    // re-armed whenever the fork lies clearly ahead of the kart again - next lap, a pause-menu
    // retry, or a Lakitu drop-back behind the entrance all re-arm naturally, so the shortcut
    // can always be re-attempted.
    std::vector<uint8_t> mAnnounceArmed;
    std::vector<uint8_t> mLatchArmed;

    // Key of the track data the routes were built from (rebuild when any of these change).
    const void* mBuiltPath = nullptr; // raw PathTable2[1] pointer
    int mBuiltMirror = -1;            // gIsMirrorMode
    int mBuiltMainCount = -1;         // gPathCountByPathIndex[0]

    int mFollowRoute = -1; // route the carrot is currently leading along, -1 if none
    int mExitChords = 0;   // "made it through" chords still to ring after finishing a route
    int mLatchGrace = 0;   // ticks after latching during which the release check stays off
    int mTimer = 0;        // ticks until the next beep / chord
    int mPrevMain = -1;    // previous frame's nearest main waypoint (backwards-warp detection)
    int mSpeakCooldown = 0; // ticks until the spoken heads-up may fire again (anti-repeat)

    // Wario Stadium wall-jump state. The launch waypoint sits ON the main road right under
    // the lap's overpass; riding the banked wall there climbs onto it. The landing index is
    // derived from the path data at load, and making the climb shows up as a one-frame
    // forward leap in the kart's main-path progress.
    bool mWarioAnnounceArmed = true; // heads-up may speak; re-armed once the launch is ahead again
    int mWarioPrevMain = -1;         // previous frame's nearest main waypoint (leap detection)
    int mWarioLandIdx = -1;          // main-path index of the overpass landing (derived at load)

    float mSteerTarget[3] = { 0.0f, 0.0f, 0.0f }; // current carrot, for the steering guide
    bool mSteerActive = false;                    // whether mSteerTarget is valid this frame
};
