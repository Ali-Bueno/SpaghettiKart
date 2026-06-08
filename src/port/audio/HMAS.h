#pragma once

typedef int HMAS_AudioId;

enum HMAS_ChannelId {
    HMAS_MUSIC,
    HMAS_SFX,
    HMAS_ENV,
    HMAS_ACCESS, // accessibility item-box beacon: its own channel so it never cuts the cue beeps
    HMAS_SHELL,     // accessibility spinning-shell loop (green / blue): own channel so it overlaps the beacon / cues
    HMAS_SHELL_RED, // accessibility spinning red-shell loop: own channel so it overlaps the green/blue shell loop
    HMAS_BANANA,    // accessibility grounded-banana hazard blip: own channel so it overlaps the others
    HMAS_OBSTACLE,  // accessibility obstacle collision-warning blip: own channel so it overlaps the others
    HMAS_MAX_CHANNELS
};

enum HMAS_EffectType {
    HMAS_EFFECT_VOLUME,
    HMAS_EFFECT_PITCH,
    HMAS_EFFECT_PAUSE,
    HMAS_EFFECT_STOP
};

enum HMAS_EffectTransition {
    HMAS_LINEAR,
    HMAS_INSTANT
};

#ifdef __cplusplus

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include "audio/miniaudio.h"

struct HMAS_Loop {
    int64_t start;
    int64_t end;
};

struct HMAS_Info {
    HMAS_Loop loop = { -1, -1 };
    std::string name;
    std::string author;
    std::string date;
};

struct HMAS_Sample {
    ma_sound sound;
    ma_decoder decoder;

    HMAS_Info info;
};

struct HMAS_Effect {
    HMAS_EffectType type;
    HMAS_EffectTransition transition;
    uint32_t numFrames;
    float target;
};

struct HMAS_ChannelInfo {
    ma_sound* sound;

    ma_uint64 cursor;
    float pitch;
    float volume;

    std::vector<HMAS_Effect> effects;
};

class HMAS {
public:
    HMAS();
    ~HMAS();

    void RegisterSound(HMAS_AudioId id, const std::string& filePath, HMAS_Info info = {});
    void RegisterSound(HMAS_AudioId id, uint8_t* data, uint32_t size, HMAS_Info info = {});

    void Play(HMAS_ChannelId channel, HMAS_AudioId id, bool loop = false);
    void Stop(HMAS_ChannelId channel);
    bool IsPlaying(HMAS_ChannelId channel);

    void SetPitch(HMAS_ChannelId channel, float pitch);
    void SetVolume(HMAS_ChannelId channel, float volume);
    void RefreshMusicVolume(); // re-apply the music channel volume from gMainMusicVolume
    void SetPan(HMAS_ChannelId channel, float pan); // -1 = left, 0 = center, +1 = right
    void SetPause(HMAS_ChannelId channel, bool pause);
    void AddEffect(HMAS_ChannelId channel, HMAS_EffectType type, HMAS_EffectTransition transition, uint32_t frames, float target);

    bool IsIDRegistered(HMAS_AudioId id);

    void ProcessEffects();
    void CreateBuffer(uint8_t* samples, uint32_t num_samples);

    float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

private:
    ma_engine gAudioEngine;
    HMAS_ChannelInfo gChannelSound[HMAS_MAX_CHANNELS] = { 0 };
    std::unordered_map<HMAS_AudioId, HMAS_Sample> gRegistry;
};

extern "C" {
#endif

void HMAS_Play(enum HMAS_ChannelId channel, HMAS_AudioId id, bool loop);
void HMAS_Stop(enum HMAS_ChannelId channel);
bool HMAS_IsPlaying(enum HMAS_ChannelId channel);
void HMAS_SetPitch(enum HMAS_ChannelId channel, float pitch);
void HMAS_SetVolume(enum HMAS_ChannelId channel, float volume);
void HMAS_RefreshMusicVolume(void);
void HMAS_SetPause(enum HMAS_ChannelId channel, bool pause);
void HMAS_AddEffect(enum HMAS_ChannelId channel, enum HMAS_EffectType type, enum HMAS_EffectTransition transition, uint32_t frames, float target);
bool HMAS_IsIDRegistered(HMAS_AudioId id);

#ifdef __cplusplus
}
#endif
