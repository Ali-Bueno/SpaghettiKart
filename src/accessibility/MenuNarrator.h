#pragma once

#include <string>

class ScreenReaderService;

/**
 * Narrates front-end menu navigation.
 *
 * Every frame it builds a short text describing the currently highlighted item
 * (screen + sub-state + cursor) and speaks it whenever it changes, plus the
 * screen name when the player moves to a different menu screen.
 *
 * It only reads game state and delegates to ScreenReaderService; it holds no
 * game logic and never alters menu behaviour.
 */
class MenuNarrator {
  public:
    // Forget the last narrated state (call when leaving the front-end).
    void Reset();
    // Inspect menu state and speak any change. Called once per frame while the
    // game is in a menu context.
    void Tick(ScreenReaderService& reader);

  private:
    const char* ScreenName(int screen) const;
    // Label for the currently highlighted item on the given screen, or "" if none.
    std::string BuildItemAnnouncement(int screen) const;

    int mLastScreen = -1;
    std::string mLastAnnouncement;
};
