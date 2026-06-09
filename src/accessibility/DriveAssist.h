#pragma once

#include <string>
#include <vector>

class ScreenReaderService;

/**
 * Blind drive assist (Forza-style).
 *
 * The track is reduced once per load to a "curve map": the path is walked using the
 * game's own normalized curvature to find each curve, and each curve is stored with
 * absolute path-point landmarks (entry / apex / exit), a direction and a geometry-graded
 * severity. A cumulative arc-length table makes every distance the assist uses a real
 * world distance instead of a count of (non-uniformly spaced) path points - so the cues
 * behave the same on every track and the same curve is announced consistently every lap.
 *
 *  1. Curve announcement (speech): a graded rally-style call - "Left", "Hard right",
 *     "Hairpin left", "Easy right long" - spoken once as each curve comes into range,
 *     re-armed every lap.
 *  2. Approach beeps: up to three rising-pitch beeps as the curve entry nears (by distance).
 *  3. Curve-progress beeps: entry, apex (same pitch) and a higher-pitched exit, each fired
 *     once at the curve's stored landmarks. Back-to-back curves each get their own set.
 *  4. Steering Guide: the player's engine audio is panned toward the side to steer, so the
 *     player drives TOWARD the sound (Accessibility_SetKartAudioPan). It blends look-ahead
 *     pure pursuit (aim at a point a fixed distance ahead on the racing line) with a lateral
 *     centering term (pull back toward the middle of the track). Look-ahead distance is the
 *     "anticipation" CVar, centering strength its own CVar, and Invert flips the side.
 *  5. Edge-proximity cue: silent while centered, then beeps panned toward the nearer edge
 *     that get faster/higher as you drift out, becoming a held tone at the limit.
 */
class DriveAssist {
  public:
    void Reset();
    // steerTarget (optional): a world point the steering guide should aim at INSTEAD of the
    // main racing line - the ShortcutBeacon supplies its moving point on the shortcut route
    // while it leads the kart through one. While overridden, the main-path-relative cues
    // (curve calls, approach/traversal beeps, edge cue) pause: they describe exactly the line
    // the kart is deliberately leaving and would fight the shortcut guidance.
    void Tick(ScreenReaderService& reader, const float* steerTarget = nullptr);

  private:
    // Geometry-graded curve tightness. Normal has no spoken prefix.
    enum class Severity { Normal, Easy, Hard, Hairpin };

    struct Curve {
        int entry = 0; // path point index where the curve begins
        int apex = 0;  // path point index of the tightest part
        int exit = 0;  // path point index where the curve ends
        bool right = false;
        Severity severity = Severity::Normal;
        bool isLong = false;
    };

    void RebuildCurveMap(int pathIndex, int count);
    // Signed forward arc distance (world units) from point `from` to point `to`, in
    // (-total/2, total/2]; positive = `to` is ahead. Uses the cumulative-length table so it
    // does not depend on the (non-uniform) path-point spacing.
    float ArcForward(int from, int to) const;
    // Spoken call for one curve, e.g. "Hard left", "Easy right long".
    std::string CurvePhrase(const Curve& c) const;

    // Curve map, rebuilt when the track / path changes.
    const void* mMapKey = nullptr; // gTrackPaths[pathIndex] the map was built from
    int mMapPathIndex = -1;
    int mMapCount = 0;
    float mAvgSpacing = 1.0f;    // mean point spacing (units): anchors look-ahead/announce in distance
    std::vector<float> mCumDist; // cumulative arc length per point (size count + 1)
    std::vector<Curve> mCurves;  // curves around the loop, in path order

    int mActiveCurve = -1;       // curve being approached / driven (index into mCurves)
    std::vector<bool> mAnnounced; // per-curve: already spoken this lap (sized to mCurves)
    int mLastLap = -1;           // player lap, to re-arm announcements every lap
    int mApproachBeeps = 0;   // approach beeps played for the active curve
    int mCurvePhase = 0;      // traversal-beep state for the active curve (0..3)
    int mEdgeBeepTimer = 0;   // ticks until the next edge beep
    float mSmoothedPan = 0.0f;// low-pass filtered engine pan
};
