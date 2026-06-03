#include "AudioCueService.h"

#include "port/Engine.h"
#include "port/audio/HMAS.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr HMAS_AudioId kApproachBeepId = 0x40ACCE51;
constexpr HMAS_AudioId kCurveBeepId = 0x40ACCE52;
constexpr HMAS_AudioId kEdgeBeepId = 0x40ACCE53;
constexpr HMAS_AudioId kEdgeToneId = 0x40ACCE54;
// Curve-related cues and the edge cue live on separate channels so a continuous
// edge tone never cuts the curve beeps (and vice versa). The game itself only
// uses HMAS_MUSIC, so HMAS_ENV and HMAS_SFX are free for accessibility cues.
constexpr HMAS_ChannelId kCurveChannel = HMAS_ENV; // approach + curve-progress beeps
constexpr HMAS_ChannelId kEdgeChannel = HMAS_SFX;  // edge beeps + held edge tone

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
        BuildBeepWav(mApproachWav, 700.0f, 2600, Wave::Sine);        // curve approach: smooth sine
        BuildBeepWav(mCurveWav, 480.0f, 3000, Wave::Triangle);       // curve entry/apex/exit: soft triangle
        BuildBeepWav(mEdgeWav, 900.0f, 850, Wave::Saw);              // edge proximity: harsh saw (short, stays crisp when rapid)
        BuildBeepWav(mEdgeToneWav, 1000.0f, 3200, Wave::Saw, false); // held edge tone: saw, seamless loop
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
    switch (kind) {
        case CueBeep::Approach: id = kApproachBeepId; break;
        case CueBeep::Curve:    id = kCurveBeepId; break;
        case CueBeep::Edge:     id = kEdgeBeepId; channel = kEdgeChannel; break;
        default: return;
    }

    hmas->Play(channel, id, false);
    hmas->SetPan(channel, std::clamp(pan, -1.0f, 1.0f));
    hmas->SetPitch(channel, std::clamp(pitch, 0.25f, 3.0f));
    hmas->SetVolume(channel, kBeepVolume);
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
        hmas->SetPan(kEdgeChannel, std::clamp(pan, -1.0f, 1.0f));
        hmas->SetPitch(kEdgeChannel, std::clamp(pitch, 0.25f, 3.0f));
        hmas->SetVolume(kEdgeChannel, kBeepVolume);
    } else if (mEdgeTonePlaying) {
        hmas->Stop(kEdgeChannel);
        mEdgeTonePlaying = false;
    }
}
