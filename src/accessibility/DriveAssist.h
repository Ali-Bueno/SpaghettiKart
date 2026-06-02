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
 *  4. Directional reference: the player's engine audio is panned
 *     (Accessibility_SetKartAudioPan). Two selectable models (CVAR_..._PAN_MODE):
 *       - Lateral position (default, Top Speed style): pans toward the edge you
 *         have drifted to; centered = on the racing line. A curve moves the line
 *         under you, so the pan leans outward until you steer to follow it.
 *       - Heading error: pans toward the side to steer so the path ahead lines
 *         up with the kart's facing.
 *  5. Edge-proximity cue: a short beep, panned to the side, when you drift close
 *     to a track edge - a warning before going off-road.
 */
class DriveAssist {
  public:
    void Reset();
    void Tick(ScreenReaderService& reader);

  private:
    bool mCurveAnnounced = false; // a curve ahead has been announced (episode)
    int mApproachBeeps = 0;       // approach beeps played for the current curve
    bool mWasInCurve = false;     // for entry/exit progress beeps
    bool mWasNearEdge = false;    // hysteresis latch for the edge-proximity cue
    float mSmoothedPan = 0.0f;    // low-pass filtered engine pan (avoids abrupt jumps)
};
