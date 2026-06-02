#pragma once

class ScreenReaderService;

/**
 * Blind drive assist (Forza-style, basic).
 *
 * Layers 1-3 are derived from the track's precomputed path heading
 * (gPathExpectedRotation); the rest use the player's signed lateral position on
 * the track (calculate_track_position_factor, the same value the game feeds its
 * AI: -1 = left edge, 0 = center, +1 = right edge):
 *  1. Curve announcement (speech): "Left" / "Right" / "Hard left/right",
 *     announced once per curve when it comes into range.
 *  2. Approach beeps: up to three rising-pitch centered beeps as the curve
 *     entry gets nearer.
 *  3. Curve-progress beeps: a distinct timbre at curve entry and exit.
 *  4. Steering Guide: the player's engine audio is panned toward the side to steer
 *     to follow the racing line, so the player drives TOWARD the sound
 *     (Accessibility_SetKartAudioPan). Two models (CVAR_..._PAN_MODE):
 *       - Racing line / pure-pursuit (default): aims at a look-ahead point on the
 *         line and pans by (bearing - heading), so it both recenters you onto the
 *         line and anticipates the curve. Mirrors the game's own AI steering.
 *       - Heading only: pans to align the kart's facing with the path ahead
 *         (curve anticipation, no lateral centering).
 *     Look-ahead distance is the "anticipation" (CVAR_..._LOOKAHEAD); Invert flips
 *     the side.
 *  5. Edge-proximity cue: as you drift toward a track edge the cue beeps panned to
 *     that side, getting faster and higher-pitched the closer you get, and turning
 *     into a steady held tone right at the limit - a continuous off-road warning.
 */
class DriveAssist {
  public:
    void Reset();
    void Tick(ScreenReaderService& reader);

  private:
    bool mCurveAnnounced = false; // a curve ahead has been announced (episode)
    int mApproachBeeps = 0;       // approach beeps played for the current curve
    bool mWasInCurve = false;     // for entry/exit progress beeps (hysteretic)
    int mEdgeBeepTimer = 0;       // ticks until the next edge beep (rate scales with closeness)
    float mSmoothedPan = 0.0f;    // low-pass filtered engine pan (avoids abrupt jumps)
};
