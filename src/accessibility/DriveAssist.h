#pragma once

class ScreenReaderService;

/**
 * Blind drive assist (Forza-style, basic).
 *
 * All layers are derived from the track's precomputed path heading
 * (gPathExpectedRotation) versus the player's facing:
 *  1. Curve announcement (speech): "Left" / "Right" / "Hard left/right",
 *     announced once per curve when it comes into range.
 *  2. Approach beeps: up to three rising-pitch centered beeps as the curve
 *     entry gets nearer.
 *  3. Curve-progress beeps: a distinct timbre at curve entry and exit.
 *  4. Directional reference: the player's engine audio is panned toward the
 *     side to steer (Accessibility_SetKartAudioPan); centered = aligned.
 */
class DriveAssist {
  public:
    void Reset();
    void Tick(ScreenReaderService& reader);

  private:
    bool mCurveAnnounced = false; // a curve ahead has been announced (episode)
    int mApproachBeeps = 0;       // approach beeps played for the current curve
    bool mWasInCurve = false;     // for entry/exit progress beeps
    float mSmoothedPan = 0.0f;    // low-pass filtered engine pan (avoids abrupt jumps)
};
