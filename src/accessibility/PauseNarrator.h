#pragma once

#include <string>

class ScreenReaderService;

/**
 * Narrates the in-race PAUSE menu.
 *
 * The pause menu appears while gGamestate == RACING (gIsGamePaused != 0). It is a
 * separate menu from the front-end screens handled by MenuNarrator, with its own
 * per-game-mode option list, so it gets its own narrator.
 *
 * Like the other narrators it only reads game state and delegates to
 * ScreenReaderService; it holds no game logic and never alters menu behaviour.
 */
class PauseNarrator {
  public:
    // Forget the last narrated state (call when leaving the pause menu / race).
    void Reset();
    // Inspect the pause-menu state and speak any change. Called once per frame
    // while the game is paused during a race.
    void Tick(ScreenReaderService& reader);

  private:
    // Label of the highlighted pause option for the current mode, or "" if none.
    std::string CurrentOption() const;

    bool mWasPaused = false;
    std::string mLastAnnouncement;
};
