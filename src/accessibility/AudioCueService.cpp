#include "AudioCueService.h"

#include "port/Engine.h"
#include "port/audio/HMAS.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr HMAS_AudioId kApproachBeepId = 0x40ACCE51;
constexpr HMAS_AudioId kCurveBeepId = 0x40ACCE52;
constexpr HMAS_ChannelId kBeepChannel = HMAS_ENV;

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

// Build a 16-bit mono PCM WAV of a sine tone with a short attack/release ramp so
// one-shot beeps don't click.
void BuildBeepWav(std::vector<uint8_t>& out, float freq, int samples) {
    const uint32_t dataSize = static_cast<uint32_t>(samples) * 2;
    out.clear();
    out.reserve(44 + dataSize);

    const char* riff = "RIFF";
    out.insert(out.end(), riff, riff + 4);
    PushU32(out, 36 + dataSize);
    const char* wave = "WAVE";
    out.insert(out.end(), wave, wave + 4);
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

    const int ramp = samples / 8;
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        double s = std::sin(2.0 * M_PI * freq * t);
        double gain = 1.0;
        if (i < ramp) {
            gain = static_cast<double>(i) / ramp;
        } else if (i > samples - ramp) {
            gain = static_cast<double>(samples - i) / ramp;
        }
        const int16_t sample = static_cast<int16_t>(s * gain * 12000.0);
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
        BuildBeepWav(mApproachWav, 760.0f, 2600); // bright, short
        BuildBeepWav(mCurveWav, 420.0f, 3000);    // lower, distinct timbre
    }
    HMAS* hmas = GameEngine::Instance->gHMAS;
    if (!hmas->IsIDRegistered(kApproachBeepId)) {
        hmas->RegisterSound(kApproachBeepId, mApproachWav.data(), static_cast<uint32_t>(mApproachWav.size()));
    }
    if (!hmas->IsIDRegistered(kCurveBeepId)) {
        hmas->RegisterSound(kCurveBeepId, mCurveWav.data(), static_cast<uint32_t>(mCurveWav.size()));
    }
    mReady = hmas->IsIDRegistered(kApproachBeepId);
    return mReady;
}

void AudioCueService::PlayBeep(CueBeep kind, float pitch) {
    if (!EnsureInitialized()) {
        return;
    }
    HMAS* hmas = GameEngine::Instance->gHMAS;
    const HMAS_AudioId id = (kind == CueBeep::Approach) ? kApproachBeepId : kCurveBeepId;

    hmas->Play(kBeepChannel, id, false);
    hmas->SetPan(kBeepChannel, 0.0f); // beeps are always centered
    hmas->SetPitch(kBeepChannel, std::clamp(pitch, 0.25f, 3.0f));
    hmas->SetVolume(kBeepChannel, kBeepVolume);
}
