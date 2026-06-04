#pragma once

#include <string>

class ScreenReaderService;

/**
 * Narrates the post-race sequence (gGamestate == ENDING):
 *   - Versus / Battle: the selectable results menu (MENU_ITEM_TYPE_0B0, the
 *     Retry / Course change / Driver change / Quit options), and
 *   - Grand Prix: the player's finishing position when the race-results screen
 *     appears (the GP standings auto-advance, so there is no menu to navigate).
 *
 * Like the other narrators it only reads game state and delegates to
 * ScreenReaderService; it holds no game logic and never alters menu behaviour.
 */
class PostRaceNarrator {
  public:
    // Forget the last narrated state (call when leaving the post-race sequence).
    void Reset();
    // Inspect the post-race state and speak any change. Called once per frame
    // while gGamestate == ENDING.
    void Tick(ScreenReaderService& reader);

  private:
    // Highlighted Versus/Battle option label, or "" if that menu isn't selectable.
    std::string VersusBattleOption() const;

    int mLastKind = 0;           // 0 none, 1 versus/battle menu, 2 GP result
    std::string mLastAnnouncement;
    int mLastGpRank = -1;        // last announced Grand Prix finishing rank
};
