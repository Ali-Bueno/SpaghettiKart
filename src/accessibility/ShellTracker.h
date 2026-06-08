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
 * The red shell is tracked separately from the green / blue shells and uses its own
 * distinct sound, so the homing red shell is recognizable by ear; both groups behave
 * identically and play on their own channels, so a red and a green shell can be heard
 * at once.
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
    // Silence both loops (call when leaving the live race).
    void Reset();
    // Find the nearest flying red shell and the nearest flying green/blue shell and drive
    // their looping cues. Once per frame.
    void Tick();

  private:
    // Drive one loop (red=true for the red shell, false for green/blue) from the nearest
    // matching shell this frame: found=false keeps the loop alive briefly then stops it;
    // found=true steers its pan/volume/pitch from the shell distance and bearing error.
    void DriveVoice(bool red, bool found, float dist, int errorAngle);

    // Keep each loop alive this many ticks after its last moving shell (anti-flicker), so a
    // one-frame velocity dip does not click it off. Index 0 = green/blue, 1 = red.
    int mHoldTimer[2] = { 0, 0 };
};
