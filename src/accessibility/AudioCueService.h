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

    // Looping whoosh for a green / blue shell in flight (thrown by anyone), on its own channel
    // so it can overlap the cue beeps and the item-box beacon. on=true starts the loop once and
    // then just steers its pan/volume/pitch each frame; on=false stops it. Idempotent, same
    // Doppler/pan model as the item-box beacon. Loads SE_ITM_KAME_G_MOVE.wav on first use;
    // a no-op if that file is missing.
    void SetShellLoop(bool on, float pan, float volume, float pitch);
    // Stop the green / blue shell loop (e.g. when no such shell is moving or the race ends).
    void StopShellLoop();

    // Looping whoosh for a red shell in flight, on its own channel so it overlaps (and is
    // distinct from) the green/blue shell loop. Same behavior as SetShellLoop, but a separate
    // sound (SE_ITM_KAME_R_MOVE.wav) so the homing red shell is recognizable by ear.
    void SetShellRedLoop(bool on, float pan, float volume, float pitch);
    // Stop the red shell loop (e.g. when no red shell is moving or the race ends).
    void StopShellRedLoop();

    // One-shot hazard blip toward a banana resting on the track, on its own channel.
    // pan -1..+1, volume 0..1, pitch multiplies the base frequency (lower once the banana
    // is behind you - the same Doppler cue as the item-box beacon). Retriggered by
    // BananaBeacon every ~600 ms. Loads "banana in ground.wav" on first use; a no-op if
    // that file is missing.
    void PlayBananaBeacon(float pan, float volume, float pitch = 1.0f);
    // Cut any in-progress banana blip (e.g. when no banana is near or the race ends).
    void StopBananaBeacon();

    // One-shot collision-warning blip toward the nearest in-range obstacle (traffic, falling
    // rock, train...), on its own channel. pan -1..+1, volume 0..1, pitch multiplies the base
    // frequency (lower once the obstacle is behind you - same Doppler cue). Retriggered by
    // ObstacleBeacon every ~200 ms. Loads SE_ITM_EQUIP.wav on first use; a no-op if missing.
    void PlayObstacleBeacon(float pan, float volume, float pitch = 1.0f);
    // Cut any in-progress obstacle blip (e.g. when nothing is in range or the race ends).
    void StopObstacleBeacon();

    // One-shot guidance beep toward a shortcut entrance or the recommended fork route, on its
    // own channel. A bright procedural tone (no game sound), panned toward the target and rising
    // in pitch as you near it. pan -1..+1, pitch multiplies the base frequency. Used by both the
    // ShortcutBeacon (fixed shortcuts) and the Yoshi Valley MultiPathGuide (fork routing).
    void PlayShortcutBeep(float pitch, float pan);
    // One-shot "take it now" chord (a perfect fifth) played repeatedly while the kart sits right
    // on a shortcut entry, so it reads clearly as "you are on the spot". pan -1..+1.
    void PlayShortcutHit(float pan);
    // Cut any in-progress shortcut / route guidance cue (e.g. out of range or the race ends).
    void StopShortcutCue();

    // One-shot CENTERED fork alert (the same chord, on its own channel and never panned): a
    // non-directional "fork ahead" beep for Yoshi Valley, played alongside the spoken heads-up so
    // the cue still lands if the speech is masked by engine/race noise. Centered on purpose - a
    // panned route cue competed with the engine-pan steering guide and confused the player.
    void PlayForkAlert();

  private:
    AudioCueService() = default;

    bool EnsureInitialized();
    bool EnsureBeaconLoaded();
    bool EnsureShellLoaded();
    bool EnsureShellRedLoaded();
    bool EnsureBananaLoaded();
    bool EnsureObstacleLoaded();
    // Build + register the two procedural shortcut cues (the beep and the fifth chord) once.
    bool EnsureShortcutLoaded();
    // Drive a looping cue on its own channel: start it once when it turns on, then just steer
    // pan/volume/pitch; stop it when it turns off. Shared by both shell loops. `ready` is the
    // result of the matching Ensure* call (ignored when on=false); `playing` is the per-loop
    // started flag.
    void DriveLoop(int channel, int id, bool ready, bool& playing, bool on, float pan,
                   float volume, float pitch);
    // Load a raw sound packed into spaghetti.o2r and register it with HMAS from memory.
    // The decoded bytes are referenced by miniaudio, so they are kept alive in keepAlive
    // (an AudioCueService member that lives for the whole session). Idempotent.
    bool EnsureArchiveSound(bool& ready, bool& failed, int id, const char* path,
                            std::vector<uint8_t>& keepAlive);

    bool mReady = false;
    bool mEdgeTonePlaying = false; // whether the looping edge tone is currently playing
    bool mBeaconReady = false;     // whether the item-box beacon sound is loaded
    bool mBeaconLoadFailed = false; // file missing: don't keep retrying every frame
    bool mShellReady = false;       // whether the green/blue spinning-shell loop sound is loaded
    bool mShellLoadFailed = false;  // file missing: don't keep retrying every frame
    bool mShellLoopPlaying = false; // whether the green/blue shell whoosh is currently playing
    bool mShellRedReady = false;       // whether the red spinning-shell loop sound is loaded
    bool mShellRedLoadFailed = false;  // file missing: don't keep retrying every frame
    bool mShellRedLoopPlaying = false; // whether the red shell whoosh is currently playing
    bool mBananaReady = false;      // whether the grounded-banana hazard sound is loaded
    bool mBananaLoadFailed = false; // file missing: don't keep retrying every frame
    bool mObstacleReady = false;      // whether the obstacle collision-warning sound is loaded
    bool mObstacleLoadFailed = false; // file missing: don't keep retrying every frame
    bool mShortcutReady = false;      // whether the procedural shortcut cues are built/registered

    // WAV buffers kept alive: the miniaudio decoders reference them. The generated cue
    // beeps and the sounds loaded from spaghetti.o2r both live here for the whole session.
    std::vector<uint8_t> mApproachWav;
    std::vector<uint8_t> mCurveWav;
    std::vector<uint8_t> mEdgeWav;
    std::vector<uint8_t> mEdgeToneWav;
    std::vector<uint8_t> mBeaconBytes;  // item-box beacon WAV (from the archive)
    std::vector<uint8_t> mShellBytes;   // green/blue spinning-shell loop WAV (from the archive)
    std::vector<uint8_t> mShellRedBytes; // red spinning-shell loop WAV (from the archive)
    std::vector<uint8_t> mBananaBytes;  // grounded-banana WAV (from the archive)
    std::vector<uint8_t> mObstacleBytes; // obstacle collision-warning WAV (from the archive)
    std::vector<uint8_t> mShortcutBeepWav; // procedural shortcut/route guidance beep
    std::vector<uint8_t> mShortcutHitWav;  // procedural "take it now" fifth chord
};
