#pragma once

class ScreenReaderService;

/**
 * Advance fork warning for Yoshi Valley - the only track that splits into four separate
 * racing paths through its middle section.
 *
 * A blind player cannot see the split coming, so this speaks a short heads-up a little before
 * the kart reaches the fork (while still on the single approach path), once per lap. It makes
 * no sound and never changes the steering: the player then navigates the split with the normal
 * engine-pan steering guide, which already follows whichever branch they take. (A panned cue
 * toward a specific branch was tried and removed - it competed with the engine pan and confused
 * the player; a plain advance warning is what was asked for.)
 *
 * Like the rest of the drive assist it only runs during the live race; AccessibilityManager
 * gates that and calls Reset() otherwise.
 */
class MultiPathGuide {
  public:
    // Re-arm the warning (call when leaving the live race).
    void Reset();
    // Speak the heads-up once as the kart approaches the fork. Once per frame.
    void Tick(ScreenReaderService& reader);

  private:
    bool mWarned = false; // already announced the upcoming fork this lap
    int mLastLap = -1;    // player lap, to re-arm the warning every lap
};
