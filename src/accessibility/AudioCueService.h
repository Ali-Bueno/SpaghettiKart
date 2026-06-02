#pragma once

#include <cstdint>
#include <vector>

/**
 * Non-speech audio cues for accessibility, layered on the game's HMAS engine.
 *
 *  - Beeps: short one-shot cues (HMAS ENV channel) with distinct timbres for
 *    curve approach, curve progress, and edge proximity, re-pitched per event.
 *    Curve beeps are centered; the edge beep is panned to the side it warns of.
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

  private:
    AudioCueService() = default;

    bool EnsureInitialized();

    bool mReady = false;

    // WAV buffers kept alive: the miniaudio decoders reference them.
    std::vector<uint8_t> mApproachWav;
    std::vector<uint8_t> mCurveWav;
    std::vector<uint8_t> mEdgeWav;
};
