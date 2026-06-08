#include "AudioCueService.h"

#include "AccessibilityCVars.h"
#include "port/Engine.h"
#include "port/audio/HMAS.h"

#include <libultraship.h>
#include "ship/Context.h"
#include "ship/resource/ResourceManager.h"
#include "ship/resource/File.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace {

constexpr HMAS_AudioId kApproachBeepId = 0x40ACCE51;
constexpr HMAS_AudioId kCurveBeepId = 0x40ACCE52;
constexpr HMAS_AudioId kEdgeBeepId = 0x40ACCE53;
constexpr HMAS_AudioId kEdgeToneId = 0x40ACCE54;
constexpr HMAS_AudioId kItemBoxBeaconId = 0x40ACCE55;
constexpr HMAS_AudioId kShellLoopId = 0x40ACCE56;
constexpr HMAS_AudioId kBananaBeaconId = 0x40ACCE57;
constexpr HMAS_AudioId kShellRedLoopId = 0x40ACCE58;
constexpr HMAS_AudioId kObstacleBeaconId = 0x40ACCE59;
// Curve-related cues and the edge cue live on separate channels so a continuous
// edge tone never cuts the curve beeps (and vice versa). The game itself only
// uses HMAS_MUSIC, so HMAS_ENV, HMAS_SFX and HMAS_ACCESS are free for our cues.
constexpr HMAS_ChannelId kCurveChannel = HMAS_ENV;     // approach + curve-progress beeps
constexpr HMAS_ChannelId kEdgeChannel = HMAS_SFX;      // edge beeps + held edge tone
constexpr HMAS_ChannelId kBeaconChannel = HMAS_ACCESS; // item-box proximity beacon
constexpr HMAS_ChannelId kShellChannel = HMAS_SHELL;        // green/blue spinning-shell loop
constexpr HMAS_ChannelId kShellRedChannel = HMAS_SHELL_RED; // red spinning-shell loop
constexpr HMAS_ChannelId kBananaChannel = HMAS_BANANA;      // grounded-banana hazard blip
constexpr HMAS_ChannelId kObstacleChannel = HMAS_OBSTACLE;  // obstacle collision-warning blip

// Sounds packed into spaghetti.o2r and loaded from the game archive (not loose files), so
// players cannot swap them - keeping the authentic Nintendo-style cues intact. These are
// the virtual paths inside the archive. Float/PCM WAV both work via miniaudio.
constexpr char kItemBoxBeaconFile[] = "sounds/SE_ITM_BOX_BRK.wav";
constexpr char kShellLoopFile[] = "sounds/SE_ITM_KAME_G_MOVE.wav";
constexpr char kShellRedLoopFile[] = "sounds/SE_ITM_KAME_R_MOVE.wav";
constexpr char kBananaBeaconFile[] = "sounds/SE_ITM_BANANA_GROUND.wav";
constexpr char kObstacleBeaconFile[] = "sounds/SE_ITM_EQUIP.wav";

constexpr int kSampleRate = 32000;
constexpr float kBeepVolume = 0.55f;

void PushU32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x & 0xFF);
    v.push_back((x >> 8) & 0xFF);
    v.push_back((x >> 16) & 0xFF);
    v.push_back((x >> 24) & 0xFF);
}

void PushU16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(x & 0xFF);
    v.push_back((x >> 8) & 0xFF);
}

// Oscillator waveforms. Each cue family uses a distinct timbre so they are easy to
// tell apart by ear (sine = smooth, square = hollow/buzzy, saw = bright/harsh).
enum class Wave { Sine, Square, Triangle, Saw };

// One waveform sample in [-1, 1] for a phase measured in cycles (only the
// fractional part matters).
double WaveSample(Wave wave, double phase) {
    phase -= std::floor(phase);
    switch (wave) {
        case Wave::Square:   return phase < 0.5 ? 1.0 : -1.0;
        case Wave::Triangle: return phase < 0.5 ? (4.0 * phase - 1.0) : (3.0 - 4.0 * phase);
        case Wave::Saw:      return (2.0 * phase) - 1.0;
        case Wave::Sine:
        default:             return std::sin(2.0 * M_PI * phase);
    }
}

// Build a 16-bit mono PCM WAV of a tone with the given waveform. When ramp is true
// the tone gets a short attack/release so one-shot beeps don't click. When false
// the amplitude is constant; pass a whole number of cycles for `samples` so the
// buffer can be looped seamlessly as a steady held tone.
void BuildBeepWav(std::vector<uint8_t>& out, float freq, int samples, Wave wave, bool ramp = true) {
    const uint32_t dataSize = static_cast<uint32_t>(samples) * 2;
    out.clear();
    out.reserve(44 + dataSize);

    const char* riff = "RIFF";
    out.insert(out.end(), riff, riff + 4);
    PushU32(out, 36 + dataSize);
    const char* wave4 = "WAVE";
    out.insert(out.end(), wave4, wave4 + 4);
    const char* fmt = "fmt ";
    out.insert(out.end(), fmt, fmt + 4);
    PushU32(out, 16);
    PushU16(out, 1); // PCM
    PushU16(out, 1); // mono
    PushU32(out, kSampleRate);
    PushU32(out, kSampleRate * 2);
    PushU16(out, 2);
    PushU16(out, 16);
    const char* data = "data";
    out.insert(out.end(), data, data + 4);
    PushU32(out, dataSize);

    // Harmonically rich waveforms sound louder/harsher, so trim their amplitude to
    // keep the cues roughly balanced in loudness.
    const double amplitude = (wave == Wave::Sine)       ? 12000.0
                             : (wave == Wave::Triangle) ? 11000.0
                                                        : 8500.0; // square / saw
    const int rampLen = ramp ? samples / 8 : 0;
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        const double s = WaveSample(wave, freq * t);
        double gain = 1.0;
        if (rampLen > 0) {
            if (i < rampLen) {
                gain = static_cast<double>(i) / rampLen;
            } else if (i > samples - rampLen) {
                gain = static_cast<double>(samples - i) / rampLen;
            }
        }
        const int16_t sample = static_cast<int16_t>(s * gain * amplitude);
        PushU16(out, static_cast<uint16_t>(sample));
    }
}

// User tone trim for a cue family: slider 0-100 (50 = unchanged) maps to a pitch multiplier
// of one octave down (0) .. one octave up (100), centered at 1.0. Lets a player soften a cue.
float CueUserPitch(const char* cvar, int def) {
    const int v = std::clamp(CVarGetInteger(cvar, def), 0, 100);
    return std::pow(2.0f, static_cast<float>(v - 50) / 50.0f);
}

// User volume for a cue family: slider 0-100% maps straight to a 0..1 channel volume.
float CueUserVolume(const char* cvar, int def) {
    return std::clamp(CVarGetInteger(cvar, def), 0, 100) / 100.0f;
}

} // namespace

AudioCueService& AudioCueService::Instance() {
    static AudioCueService instance;
    return instance;
}

bool AudioCueService::EnsureInitialized() {
    if (mReady) {
        return true;
    }
    if (GameEngine::Instance == nullptr || GameEngine::Instance->gHMAS == nullptr) {
        return false;
    }
    if (mApproachWav.empty()) {
        // Distinct waveform per cue family so each is unmistakable by ear.
        BuildBeepWav(mApproachWav, 700.0f, 2600, Wave::Sine);            // curve approach: smooth high sine
        BuildBeepWav(mCurveWav, 440.0f, 3000, Wave::Square);             // curve entry/apex/exit: hollow square (clearly unlike the approach)
        BuildBeepWav(mEdgeWav, 400.0f, 1500, Wave::Triangle);            // edge proximity: soft triangle (mellow, not the old harsh saw)
        BuildBeepWav(mEdgeToneWav, 400.0f, 3200, Wave::Triangle, false); // held edge tone: soft triangle, seamless loop (40 whole cycles)
    }
    HMAS* hmas = GameEngine::Instance->gHMAS;
    if (!hmas->IsIDRegistered(kApproachBeepId)) {
        hmas->RegisterSound(kApproachBeepId, mApproachWav.data(), static_cast<uint32_t>(mApproachWav.size()));
    }
    if (!hmas->IsIDRegistered(kCurveBeepId)) {
        hmas->RegisterSound(kCurveBeepId, mCurveWav.data(), static_cast<uint32_t>(mCurveWav.size()));
    }
    if (!hmas->IsIDRegistered(kEdgeBeepId)) {
        hmas->RegisterSound(kEdgeBeepId, mEdgeWav.data(), static_cast<uint32_t>(mEdgeWav.size()));
    }
    if (!hmas->IsIDRegistered(kEdgeToneId)) {
        hmas->RegisterSound(kEdgeToneId, mEdgeToneWav.data(), static_cast<uint32_t>(mEdgeToneWav.size()));
    }
    mReady = hmas->IsIDRegistered(kApproachBeepId);
    return mReady;
}

void AudioCueService::PlayBeep(CueBeep kind, float pitch, float pan) {
    if (!EnsureInitialized()) {
        return;
    }
    HMAS* hmas = GameEngine::Instance->gHMAS;
    HMAS_AudioId id;
    HMAS_ChannelId channel = kCurveChannel;
    float userPitch = 1.0f;
    float userVolume = kBeepVolume;
    switch (kind) {
        case CueBeep::Approach:
            id = kApproachBeepId;
            userPitch = CueUserPitch(CVAR_ACCESS_CUE_PITCH_APPROACH, CVAR_ACCESS_CUE_PITCH_APPROACH_DEFAULT);
            userVolume = CueUserVolume(CVAR_ACCESS_CUE_VOL_APPROACH, CVAR_ACCESS_CUE_VOL_APPROACH_DEFAULT);
            break;
        case CueBeep::Curve:
            id = kCurveBeepId;
            userPitch = CueUserPitch(CVAR_ACCESS_CUE_PITCH_CURVE, CVAR_ACCESS_CUE_PITCH_CURVE_DEFAULT);
            userVolume = CueUserVolume(CVAR_ACCESS_CUE_VOL_CURVE, CVAR_ACCESS_CUE_VOL_CURVE_DEFAULT);
            break;
        case CueBeep::Edge:
            id = kEdgeBeepId;
            channel = kEdgeChannel;
            userPitch = CueUserPitch(CVAR_ACCESS_CUE_PITCH_EDGE, CVAR_ACCESS_CUE_PITCH_EDGE_DEFAULT);
            userVolume = CueUserVolume(CVAR_ACCESS_CUE_VOL_EDGE, CVAR_ACCESS_CUE_VOL_EDGE_DEFAULT);
            break;
        default: return;
    }

    hmas->Play(channel, id, false);
    hmas->SetPan(channel, std::clamp(pan, -1.0f, 1.0f));
    hmas->SetPitch(channel, std::clamp(pitch * userPitch, 0.25f, 3.0f));
    hmas->SetVolume(channel, userVolume);
}

void AudioCueService::SetEdgeTone(bool on, float pitch, float pan) {
    if (!EnsureInitialized()) {
        return;
    }
    HMAS* hmas = GameEngine::Instance->gHMAS;
    if (on) {
        // Start the looping tone once, then just keep steering its pan/pitch as the
        // kart slides along the edge.
        if (!mEdgeTonePlaying) {
            hmas->Play(kEdgeChannel, kEdgeToneId, true);
            mEdgeTonePlaying = true;
        }
        const float userPitch = CueUserPitch(CVAR_ACCESS_CUE_PITCH_EDGE, CVAR_ACCESS_CUE_PITCH_EDGE_DEFAULT);
        const float userVolume = CueUserVolume(CVAR_ACCESS_CUE_VOL_EDGE, CVAR_ACCESS_CUE_VOL_EDGE_DEFAULT);
        hmas->SetPan(kEdgeChannel, std::clamp(pan, -1.0f, 1.0f));
        hmas->SetPitch(kEdgeChannel, std::clamp(pitch * userPitch, 0.25f, 3.0f));
        hmas->SetVolume(kEdgeChannel, userVolume);
    } else if (mEdgeTonePlaying) {
        hmas->Stop(kEdgeChannel);
        mEdgeTonePlaying = false;
    }
}

bool AudioCueService::EnsureArchiveSound(bool& ready, bool& failed, int id, const char* path,
                                         std::vector<uint8_t>& keepAlive) {
    if (ready) {
        return true;
    }
    if (failed) {
        return false; // sound not in the archive; don't retry each frame
    }
    if (GameEngine::Instance == nullptr || GameEngine::Instance->gHMAS == nullptr) {
        return false; // audio engine not up yet; try again later (not a hard failure)
    }
    auto context = Ship::Context::GetInstance();
    if (context == nullptr) {
        return false; // engine still starting; try again later
    }
    auto resourceManager = context->GetResourceManager();
    if (resourceManager == nullptr) {
        return false;
    }
    HMAS* hmas = GameEngine::Instance->gHMAS;
    if (!hmas->IsIDRegistered(id)) {
        // Read the raw WAV bytes straight out of spaghetti.o2r (bypassing resource
        // deserialization), then register them with HMAS from memory. The decoder keeps a
        // pointer into keepAlive, so it must outlive the sound - hence the long-lived member.
        std::shared_ptr<Ship::File> file = resourceManager->LoadFileProcess(std::string(path));
        if (file == nullptr || file->Buffer == nullptr || file->Buffer->empty()) {
            SPDLOG_WARN("[Accessibility] cue sound not found in archive: {}", path);
            failed = true; // not packed into spaghetti.o2r; stop retrying
            return false;
        }
        keepAlive.assign(file->Buffer->begin(), file->Buffer->end());
        hmas->RegisterSound(id, keepAlive.data(), static_cast<uint32_t>(keepAlive.size()));
    }
    ready = hmas->IsIDRegistered(id);
    return ready;
}

bool AudioCueService::EnsureBeaconLoaded() {
    return EnsureArchiveSound(mBeaconReady, mBeaconLoadFailed, kItemBoxBeaconId, kItemBoxBeaconFile, mBeaconBytes);
}

void AudioCueService::PlayItemBoxBeacon(float pan, float volume, float pitch) {
    if (!EnsureBeaconLoaded()) {
        return;
    }
    HMAS* hmas = GameEngine::Instance->gHMAS;
    hmas->Play(kBeaconChannel, kItemBoxBeaconId, false);
    hmas->SetPan(kBeaconChannel, std::clamp(pan, -1.0f, 1.0f));
    hmas->SetVolume(kBeaconChannel, std::clamp(volume, 0.0f, 1.0f));
    hmas->SetPitch(kBeaconChannel, std::clamp(pitch, 0.25f, 3.0f));
}

void AudioCueService::StopItemBoxBeacon() {
    if (!mBeaconReady) {
        return;
    }
    GameEngine::Instance->gHMAS->Stop(kBeaconChannel);
}

void AudioCueService::DriveLoop(int channel, int id, bool ready, bool& playing, bool on,
                                float pan, float volume, float pitch) {
    const HMAS_ChannelId ch = static_cast<HMAS_ChannelId>(channel);
    if (on) {
        if (!ready) {
            return;
        }
        HMAS* hmas = GameEngine::Instance->gHMAS;
        // Start the loop once (miniaudio repeats the whole file seamlessly), then just keep
        // steering its pan/volume/pitch as the shell flies around.
        if (!playing) {
            hmas->Play(ch, id, true);
            playing = true;
        }
        hmas->SetPan(ch, std::clamp(pan, -1.0f, 1.0f));
        hmas->SetVolume(ch, std::clamp(volume, 0.0f, 1.0f));
        hmas->SetPitch(ch, std::clamp(pitch, 0.25f, 3.0f));
    } else if (playing) {
        GameEngine::Instance->gHMAS->Stop(ch);
        playing = false;
    }
}

bool AudioCueService::EnsureShellLoaded() {
    return EnsureArchiveSound(mShellReady, mShellLoadFailed, kShellLoopId, kShellLoopFile, mShellBytes);
}

void AudioCueService::SetShellLoop(bool on, float pan, float volume, float pitch) {
    DriveLoop(kShellChannel, kShellLoopId, on && EnsureShellLoaded(), mShellLoopPlaying, on, pan, volume, pitch);
}

void AudioCueService::StopShellLoop() {
    SetShellLoop(false, 0.0f, 0.0f, 1.0f);
}

bool AudioCueService::EnsureShellRedLoaded() {
    return EnsureArchiveSound(mShellRedReady, mShellRedLoadFailed, kShellRedLoopId, kShellRedLoopFile,
                              mShellRedBytes);
}

void AudioCueService::SetShellRedLoop(bool on, float pan, float volume, float pitch) {
    DriveLoop(kShellRedChannel, kShellRedLoopId, on && EnsureShellRedLoaded(), mShellRedLoopPlaying, on, pan,
              volume, pitch);
}

void AudioCueService::StopShellRedLoop() {
    SetShellRedLoop(false, 0.0f, 0.0f, 1.0f);
}

bool AudioCueService::EnsureBananaLoaded() {
    return EnsureArchiveSound(mBananaReady, mBananaLoadFailed, kBananaBeaconId, kBananaBeaconFile, mBananaBytes);
}

void AudioCueService::PlayBananaBeacon(float pan, float volume, float pitch) {
    if (!EnsureBananaLoaded()) {
        return;
    }
    HMAS* hmas = GameEngine::Instance->gHMAS;
    hmas->Play(kBananaChannel, kBananaBeaconId, false);
    hmas->SetPan(kBananaChannel, std::clamp(pan, -1.0f, 1.0f));
    hmas->SetVolume(kBananaChannel, std::clamp(volume, 0.0f, 1.0f));
    hmas->SetPitch(kBananaChannel, std::clamp(pitch, 0.25f, 3.0f));
}

void AudioCueService::StopBananaBeacon() {
    if (!mBananaReady) {
        return;
    }
    GameEngine::Instance->gHMAS->Stop(kBananaChannel);
}

bool AudioCueService::EnsureObstacleLoaded() {
    return EnsureArchiveSound(mObstacleReady, mObstacleLoadFailed, kObstacleBeaconId, kObstacleBeaconFile,
                              mObstacleBytes);
}

void AudioCueService::PlayObstacleBeacon(float pan, float volume, float pitch) {
    if (!EnsureObstacleLoaded()) {
        return;
    }
    HMAS* hmas = GameEngine::Instance->gHMAS;
    hmas->Play(kObstacleChannel, kObstacleBeaconId, false);
    hmas->SetPan(kObstacleChannel, std::clamp(pan, -1.0f, 1.0f));
    hmas->SetVolume(kObstacleChannel, std::clamp(volume, 0.0f, 1.0f));
    hmas->SetPitch(kObstacleChannel, std::clamp(pitch, 0.25f, 3.0f));
}

void AudioCueService::StopObstacleBeacon() {
    if (!mObstacleReady) {
        return;
    }
    GameEngine::Instance->gHMAS->Stop(kObstacleChannel);
}
