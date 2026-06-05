#pragma once

/**
 * Audio cue for shells in flight across the track.
 *
 * While any green / red / blue shell is travelling - thrown by the player or a rival -
 * a looping whoosh plays, panned toward the shell and pitch-shifted as it falls behind
 * (the same Doppler cue as the item-box beacon), so a blind player can hear an incoming
 * shell and roughly where it is. The loop runs the sound start-to-end on repeat until no
 * shell is moving, then stops.
 *
 * A shell only counts as "thrown / spinning" once it has built up velocity: held shells,
 * the brief release wind-up and the triple shells that orbit a kart keep their position
 * locked to that kart and never gain velocity, so they are excluded.
 *
 * It only reads game state and delegates the sound to AudioCueService. Like the rest of
 * the drive assist it must only run during the live race; the AccessibilityManager gates
 * that and calls Reset() otherwise.
 */
class ShellTracker {
  public:
    // Silence the loop (call when leaving the live race).
    void Reset();
    // Find the nearest flying shell and drive the looping cue. Once per frame.
    void Tick();

  private:
    int mHoldTimer = 0; // keep the loop alive briefly after the last moving shell (anti-flicker)
};
