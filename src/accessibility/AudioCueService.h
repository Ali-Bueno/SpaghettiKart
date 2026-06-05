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

    // One-shot item-box beacon blip on its own channel: panned toward the box, scaled
    // by distance, and re-pitched (lower once the box is behind you, a Doppler "you
    // passed it" cue). pan -1..+1, volume 0..1, pitch multiplies the base frequency.
    // Retriggered by ItemBoxBeacon every ~600 ms. Loads SE_ITM_BOX_BRK.wav on first use;
    // a no-op if that file is missing.
    void PlayItemBoxBeacon(float pan, float volume, float pitch = 1.0f);
    // Cut any in-progress beacon blip (e.g. when the box is collected or the race ends).
    void StopItemBoxBeacon();

    // Looping whoosh for a shell in flight (thrown by anyone), on its own channel so it can
    // overlap the cue beeps and the item-box beacon. on=true starts the loop once and then
    // just steers its pan/volume/pitch each frame; on=false stops it. Idempotent, same
    // Doppler/pan model as the item-box beacon. Loads SE_ITM_KAME_G_MOVE.wav on first use;
    // a no-op if that file is missing.
    void SetShellLoop(bool on, float pan, float volume, float pitch);
    // Stop the shell loop (e.g. when no shell is moving or the race ends).
    void StopShellLoop();

    // One-shot hazard blip toward a banana resting on the track, on its own channel.
    // pan -1..+1, volume 0..1, pitch multiplies the base frequency (lower once the banana
    // is behind you - the same Doppler cue as the item-box beacon). Retriggered by
    // BananaBeacon every ~600 ms. Loads "banana in ground.wav" on first use; a no-op if
    // that file is missing.
    void PlayBananaBeacon(float pan, float volume, float pitch = 1.0f);
    // Cut any in-progress banana blip (e.g. when no banana is near or the race ends).
    void StopBananaBeacon();

  private:
    AudioCueService() = default;

    bool EnsureInitialized();
    bool EnsureBeaconLoaded();
    bool EnsureShellLoaded();
    bool EnsureBananaLoaded();
    // Load a raw sound packed into spaghetti.o2r and register it with HMAS from memory.
    // The decoded bytes are referenced by miniaudio, so they are kept alive in keepAlive
    // (an AudioCueService member that lives for the whole session). Idempotent.
    bool EnsureArchiveSound(bool& ready, bool& failed, int id, const char* path,
                            std::vector<uint8_t>& keepAlive);

    bool mReady = false;
    bool mEdgeTonePlaying = false; // whether the looping edge tone is currently playing
    bool mBeaconReady = false;     // whether the item-box beacon sound is loaded
    bool mBeaconLoadFailed = false; // file missing: don't keep retrying every frame
    bool mShellReady = false;       // whether the spinning-shell loop sound is loaded
    bool mShellLoadFailed = false;  // file missing: don't keep retrying every frame
    bool mShellLoopPlaying = false; // whether the looping shell whoosh is currently playing
    bool mBananaReady = false;      // whether the grounded-banana hazard sound is loaded
    bool mBananaLoadFailed = false; // file missing: don't keep retrying every frame

    // WAV buffers kept alive: the miniaudio decoders reference them. The generated cue
    // beeps and the sounds loaded from spaghetti.o2r both live here for the whole session.
    std::vector<uint8_t> mApproachWav;
    std::vector<uint8_t> mCurveWav;
    std::vector<uint8_t> mEdgeWav;
    std::vector<uint8_t> mEdgeToneWav;
    std::vector<uint8_t> mBeaconBytes; // item-box beacon WAV (from the archive)
    std::vector<uint8_t> mShellBytes;  // spinning-shell loop WAV (from the archive)
    std::vector<uint8_t> mBananaBytes; // grounded-banana WAV (from the archive)
};
