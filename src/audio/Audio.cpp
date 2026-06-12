#include "Audio.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace OS {

static AudioSystem* g_audioSystem = nullptr;

static void SDLAudioCallback(void* userdata, uint8_t* stream, int len) {
    (void)userdata;
    AudioSystem* sys = g_audioSystem;
    int numSamples = len / sizeof(int16_t);
    memset(stream, 0, len);
    if (sys) sys->FillBuffer(reinterpret_cast<int16_t*>(stream), numSamples);
}

AudioSystem::AudioSystem() {
    g_audioSystem = this;
}

AudioSystem::~AudioSystem() {
    Shutdown();
    if (g_audioSystem == this) g_audioSystem = nullptr;
}

bool AudioSystem::Init() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want = {}, got = {};
    want.freq     = AUDIO_SAMPLE_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = AUDIO_CHANNELS;
    want.samples  = AUDIO_BUFFER_SAMPLES;
    want.callback = SDLAudioCallback;

    m_deviceId = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (m_deviceId == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }

    m_emitters.resize(64);
    m_initialized = true;
    SDL_PauseAudioDevice(m_deviceId, 0);
    return true;
}

void AudioSystem::Shutdown() {
    if (!m_initialized) return;
    SDL_PauseAudioDevice(m_deviceId, 1);
    SDL_CloseAudioDevice(m_deviceId);
    m_initialized = false;
    m_sounds.clear();
    m_emitters.clear();
    m_nameToId.clear();
}

void AudioSystem::SetListener(Vec3 origin, Vec3 forward, Vec3 right, Vec3 up) {
    SDL_LockAudioDevice(m_deviceId);
    m_listener = { origin, forward, right, up };
    SDL_UnlockAudioDevice(m_deviceId);
}

// ─── WAV loading (PCM only, no ADPCM) ────────────────────────────────────────

static bool LoadWAV(const std::string& path, SoundBuffer& out) {
    SDL_AudioSpec spec;
    uint8_t* buf;
    uint32_t len;

    if (!SDL_LoadWAV(path.c_str(), &spec, &buf, &len)) {
        fprintf(stderr, "SDL_LoadWAV failed: %s: %s\n", path.c_str(), SDL_GetError());
        return false;
    }

    // Convert to S16 stereo at AUDIO_SAMPLE_RATE
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt,
        spec.format, spec.channels, spec.freq,
        AUDIO_S16SYS, AUDIO_CHANNELS, AUDIO_SAMPLE_RATE);

    if (cvt.needed) {
        cvt.len = (int)len;
        std::vector<uint8_t> cvtBuf(len * cvt.len_mult);
        memcpy(cvtBuf.data(), buf, len);
        cvt.buf = cvtBuf.data();
        SDL_ConvertAudio(&cvt);
        int numSamples = cvt.len_cvt / sizeof(int16_t);
        out.samples.resize(numSamples);
        memcpy(out.samples.data(), cvtBuf.data(), cvt.len_cvt);
    } else {
        int numSamples = len / sizeof(int16_t);
        out.samples.resize(numSamples);
        memcpy(out.samples.data(), buf, len);
    }

    out.sampleRate = AUDIO_SAMPLE_RATE;
    out.channels   = AUDIO_CHANNELS;

    SDL_FreeWAV(buf);
    return true;
}

int AudioSystem::LoadSound(const std::string& path) {
    auto it = m_nameToId.find(path);
    if (it != m_nameToId.end()) return it->second;

    SoundBuffer buf;
    buf.name = path;
    if (!LoadWAV(path, buf)) return -1;

    int id = (int)m_sounds.size();
    m_sounds.push_back(std::move(buf));
    m_nameToId[path] = id;
    return id;
}

// ─── Emitter management ───────────────────────────────────────────────────────

int AudioSystem::AllocEmitter(int channel) {
    // If channel is not AUTO, kill existing sound on that channel
    if (channel != CHAN_AUTO) {
        for (auto& em : m_emitters) {
            if (em.active && em.channel == channel) {
                em.active = false;
            }
        }
    }
    // Find free slot
    for (int i = 0; i < (int)m_emitters.size(); i++) {
        if (!m_emitters[i].active) return i;
    }
    // Steal lowest priority
    int worst = 0;
    for (int i = 1; i < (int)m_emitters.size(); i++) {
        if (m_emitters[i].priority < m_emitters[worst].priority) worst = i;
    }
    m_emitters[worst].active = false;
    return worst;
}

int AudioSystem::PlaySound(const std::string& name, Vec3 origin,
                            float volume, float attenuation,
                            int channel, float pitch) {
    if (!m_initialized) return -1;
    int soundId = LoadSound(name);
    if (soundId < 0) return -1;

    SDL_LockAudioDevice(m_deviceId);
    int slot = AllocEmitter(channel);
    SoundEmitter& em = m_emitters[slot];
    em.soundId    = soundId;
    em.origin     = origin;
    em.volume     = volume;
    em.attenuation = attenuation;
    em.channel    = channel;
    em.looping    = false;
    em.active     = true;
    em.playPos    = 0;
    em.pitch      = pitch;
    SDL_UnlockAudioDevice(m_deviceId);
    return slot;
}

void AudioSystem::StopSound(int emitterId) {
    if (emitterId < 0 || emitterId >= (int)m_emitters.size()) return;
    SDL_LockAudioDevice(m_deviceId);
    m_emitters[emitterId].active = false;
    SDL_UnlockAudioDevice(m_deviceId);
}

void AudioSystem::StopAllSounds() {
    SDL_LockAudioDevice(m_deviceId);
    for (auto& em : m_emitters) em.active = false;
    SDL_UnlockAudioDevice(m_deviceId);
}

// ─── Spatial audio ────────────────────────────────────────────────────────────

void AudioSystem::ComputeSpatial(const SoundEmitter& em,
                                  float& outLeft, float& outRight) const {
    if (em.attenuation <= 0.0f) {
        // No attenuation: global sound
        outLeft = outRight = em.volume;
        return;
    }

    Vec3 delta = em.origin - m_listener.origin;
    float dist = delta.Length();

    // Distance attenuation
    float vol = em.volume;
    if (dist > 0) {
        float attenDist = ATTN_DIST_FULL / em.attenuation;
        float maxDist   = ATTN_DIST_MAX  / em.attenuation;
        if (dist >= maxDist) {
            outLeft = outRight = 0;
            return;
        }
        if (dist > attenDist)
            vol *= 1.0f - (dist - attenDist) / (maxDist - attenDist);
    }

    // Stereo panning via dot product with listener right vector
    float pan = 0;
    if (dist > 1.0f) {
        Vec3 dir = delta.Normalized();
        pan = Clamp(dir.Dot(m_listener.right), -1.0f, 1.0f);
    }

    // Equal-power panning
    float angle = (pan + 1.0f) * 0.5f * kHalfPi;
    outLeft  = vol * std::cos(angle);
    outRight = vol * std::sin(angle);
}

// ─── Audio fill ───────────────────────────────────────────────────────────────

void AudioSystem::FillBuffer(int16_t* out, int numSamples) {
    static constexpr float kMixScale = 1.0f / 32767.0f;
    std::vector<float> mixL(numSamples / AUDIO_CHANNELS, 0.0f);
    std::vector<float> mixR(numSamples / AUDIO_CHANNELS, 0.0f);
    int frames = numSamples / AUDIO_CHANNELS;

    for (auto& em : m_emitters) {
        if (!em.active || em.soundId < 0) continue;
        const SoundBuffer& buf = m_sounds[em.soundId];
        if (buf.samples.empty()) continue;

        float volL, volR;
        ComputeSpatial(em, volL, volR);
        volL *= m_masterVolume;
        volR *= m_masterVolume;

        for (int f = 0; f < frames; f++) {
            if (em.playPos >= buf.samples.size()) {
                if (em.looping) em.playPos = 0;
                else { em.active = false; break; }
            }
            // Stereo interleaved: L at playPos, R at playPos+1
            float s = buf.samples[em.playPos] * kMixScale;
            mixL[f] += s * volL;
            mixR[f] += s * volR;
            em.playPos += buf.channels;
        }
    }

    // Clamp and write
    for (int f = 0; f < frames; f++) {
        float l = Clamp(mixL[f], -1.0f, 1.0f);
        float r = Clamp(mixR[f], -1.0f, 1.0f);
        out[f*2+0] = (int16_t)(l * 32767.0f);
        out[f*2+1] = (int16_t)(r * 32767.0f);
    }
}

} // namespace OS
