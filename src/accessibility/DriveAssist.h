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
 *  3. Curve-progress beeps: three soft beeps per curve - entry, apex (same pitch)
 *     and a higher-pitched exit - each fired exactly once as you advance through
 *     the curve. Back-to-back curves each get their own set.
 *  4. Steering Guide: the player's engine audio is panned toward the side to steer,
 *     so the player drives TOWARD the sound (Accessibility_SetKartAudioPan). Two
 *     selectable models (CVAR_..._PAN_MODE):
 *       - Curve direction (heading): pans the way the upcoming path bends.
 *         Predictable, no lateral centering.
 *       - Racing line (pure pursuit, default): aims at a look-ahead point on the
 *         line and pans by (bearing - heading); recenters onto the line AND
 *         anticipates the curve (mirrors the game's own AI).
 *     Look-ahead distance is the "anticipation" (CVAR_..._LOOKAHEAD); Invert flips
 *     the side.
 *  5. Edge-proximity cue: silent while you are comfortably centered, then beeps
 *     (panned toward the nearer edge) once you drift past the onset, getting faster
 *     and higher-pitched the closer you get and becoming a steady held tone at the
 *     limit. The onset is set by the Edge Sensitivity slider (CVAR_..._SENSITIVITY).
 */
class DriveAssist {
  public:
    void Reset();
    void Tick(ScreenReaderService& reader);

  private:
    bool mCurveAnnounced = false; // a curve ahead has been announced (episode)
    int mAnnouncedDir = 0;        // direction of the last announced curve (+1 right, -1 left); re-announce on change
    int mApproachBeeps = 0;       // approach beeps played for the current curve
    // In-curve progress beeps (Layer 3). Keyed off ABSOLUTE path-point landmarks (not
    // accumulated per-frame motion) so nearest-point jitter near the edge can't retrigger.
    int mCurvePhase = 0; // 0 armed/waiting, 1 entered (awaiting apex), 2 past apex (awaiting exit), 3 cooldown
    int mApexPoint = 0;  // path point where the apex beep fires (strongest part of the curve)
    int mExitPoint = 0;  // path point where the exit beep fires (the curve's end)
    int mEdgeBeepTimer = 0;       // ticks until the next edge beep (rate scales with closeness)
    float mSmoothedPan = 0.0f;    // low-pass filtered engine pan (avoids abrupt jumps)
};
