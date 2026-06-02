#pragma once

#include <cstdint>
#include <vector>

/**
 * Non-speech audio cues for accessibility, layered on the game's HMAS engine.
 *
 *  - Beeps: short one-shot cues with distinct timbres for curve approach, curve
 *    progress, and edge proximity, re-pitched per event. Curve cues and the edge
 *    cue play on separate channels so they never cut each other off. Curve beeps
 *    are centered; the edge cue is panned to the side it warns of and, when held
 *    right at the limit, becomes a steady looping tone (SetEdgeTone).
 *
 * The directional steering reference (panning the player's engine) is handled
 * separately by panning the kart audio source in the game audio system; see
 * Accessibility_SetKartAudioPan() in src/audio/external.c.
 */
enum class CueBeep {
    Approach, // proximity to a curve entry (rising pitch over 3 beeps)
    Curve,    // curve progress (entry / apex / exit), distinct timbre
    Edge,     // drifting close to a track edge, panned toward that edge
};

class AudioCueService {
  public:
    static AudioCueService& Instance();

    // One-shot beep. pitch multiplies the base frequency; pan is -1 (left) ..
    // +1 (right), 0 = centered.
    void PlayBeep(CueBeep kind, float pitch, float pan = 0.0f);

    // Steady looping edge tone (held right at the track limit). on=true starts/keeps
    // it playing and steers its pan/pitch; on=false stops it. Idempotent.
    void SetEdgeTone(bool on, float pitch, float pan);

  private:
    AudioCueService() = default;

    bool EnsureInitialized();

    bool mReady = false;
    bool mEdgeTonePlaying = false; // whether the looping edge tone is currently playing

    // WAV buffers kept alive: the miniaudio decoders reference them.
    std::vector<uint8_t> mApproachWav;
    std::vector<uint8_t> mCurveWav;
    std::vector<uint8_t> mEdgeWav;
    std::vector<uint8_t> mEdgeToneWav;
};
