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

    int mLastKind = 0;
    std::string mLastAnnouncement;
};
