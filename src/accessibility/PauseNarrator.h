#pragma once

#include <string>

class ScreenReaderService;

/**
 * Narrates the in-race overlay menus that appear while gGamestate == RACING:
 *   - the PAUSE menu (gIsGamePaused != 0), and
 *   - the end-of-course / replay options menu (MENU_ITEM_END_COURSE_OPTION),
 *     shown after finishing a Time Trial and when pausing during a replay.
 *
 * Both are separate from the front-end screens handled by MenuNarrator. Like the
 * other narrators it only reads game state and delegates to ScreenReaderService;
 * it holds no game logic and never alters menu behaviour.
 */
class PauseNarrator {
  public:
    // Forget the last narrated state (call when leaving the race / these menus).
    void Reset();
    // True while one of the in-race overlay menus is up. The manager uses this to
    // silence the driving cues and route narration here.
    bool MenuActive() const;
    // Inspect the active overlay menu and speak any change. Called once per frame
    // while an in-race menu is up.
    void Tick(ScreenReaderService& reader);

  private:
    // Label of the highlighted pause-menu option, or "" if none.
    std::string PauseOption() const;
    // Label of the highlighted end-course / replay option, or "" if none.
    std::string EndCourseOption() const;
    // Current option for whichever menu is active; sets *kind (0 none, 1 pause,
    // 2 end-course).
    std::string CurrentOption(int* kind) const;
    // True while the Time Trial finish option menu (0xBA) is up - used to read the lap
    // times when that screen opens (other modes' finish menus have no lap-time results).
    bool TimeTrialFinishActive() const;
    // Spoken lap times for the just-finished Time Trial: "Lap 1, t. Lap 2, t. Lap 3, t.
    // Total, t".
    std::string TimeTrialResults() const;

    int mLastKind = 0;
    std::string mLastAnnouncement;
};
