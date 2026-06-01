#pragma once

class ScreenReaderService;

/**
 * Narrates front-end menu navigation.
 *
 * Reads the game's menu state every frame and announces:
 *   - the menu screen name when the player moves to a different screen, and
 *   - the highlighted option when the cursor selection changes.
 *
 * It only delegates to ScreenReaderService; it holds no game logic and never
 * alters menu behaviour.
 */
class MenuNarrator {
  public:
    // Forget the last narrated state (call when leaving the front-end).
    void Reset();
    // Inspect menu state and speak any change. Called once per frame while the
    // game is in a menu context.
    void Tick(ScreenReaderService& reader);

  private:
    static constexpr int kNoSelection = -0x7fffffff;

    const char* ScreenName(int menuScreen) const;
    // Returns the option label for a screen/selection, or nullptr if unnamed.
    const char* OptionName(int menuScreen, int selection) const;
    // Returns the active cursor index for the given screen, or kNoSelection.
    int CurrentSelection(int menuScreen) const;

    int mLastScreen = -1;
    int mLastSelection = kNoSelection;
};
