#pragma once

class ScreenReaderService;

/**
 * Narrates live race state for player one.
 *
 * Announces changes the player cannot see: race position, lap, the item just
 * obtained, the start signal, and (optionally) leaving / returning to the road.
 *
 * It only reads game state and delegates to ScreenReaderService.
 */
class RaceNarrator {
  public:
    // Forget the last narrated state (call when leaving a race).
    void Reset();
    // Inspect race state and speak any change. Called once per frame while racing.
    void Tick(ScreenReaderService& reader);

  private:
    int mLastRank = -1;
    int mLastLap = -1;
    int mLastItem = -1;
    int mLastRaceState = -1;
    bool mWasOffRoad = false;
};
