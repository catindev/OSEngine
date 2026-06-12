#pragma once
// Audio system — SDL2-backed, mono/stereo WAV playback with 3D positioning.
// Reproduces CS 1.6 audio behavior: spatialized sound, distance attenuation.

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "../math/Math.h"

namespace OS {

static constexpr int AUDIO_SAMPLE_RATE    = 44100;
static constexpr int AUDIO_CHANNELS       = 2;     // stereo output
static constexpr int AUDIO_BUFFER_SAMPLES = 2048;

// Sound channel priorities (CS 1.6 convention)
enum SoundChannel {
    CHAN_AUTO    = 0,
    CHAN_WEAPON  = 1,
    CHAN_VOICE   = 2,
    CHAN_ITEM    = 3,
    CHAN_BODY    = 4,
    CHAN_STATIC  = 5,
    CHAN_NETWORK = 6,
    CHAN_AMBIENT = 7,
    NUM_CHANNELS = 8,
};

struct SoundBuffer {
    std::vector<int16_t> samples; // interleaved stereo
    int  sampleRate = 0;
    int  channels   = 0;
    std::string name;
};

struct SoundEmitter {
    int          soundId    = -1;
    Vec3         origin;
    float        volume     = 1.0f;
    float        attenuation = 1.0f; // ATTN_NORM = 1, ATTN_NONE = 0
    int          channel    = CHAN_AUTO;
    int          priority   = 0;
    bool         looping    = false;
    bool         active     = false;
    size_t       playPos    = 0;
    float        pitch      = 1.0f;
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool Init();
    void Shutdown();

    // Set listener position and orientation
    void SetListener(Vec3 origin, Vec3 forward, Vec3 right, Vec3 up);

    // Load a WAV file; returns sound id or -1 on failure
    int LoadSound(const std::string& path);

    // Play a sound
    // origin: world position (Vec3(0,0,0) for 2D/UI sounds with attenuation=0)
    // attenuation: 0=no falloff, 1=normal, 2=long range
    int PlaySound(const std::string& name, Vec3 origin,
                  float volume = 1.0f, float attenuation = 1.0f,
                  int channel = CHAN_AUTO, float pitch = 1.0f);

    void StopSound(int emitterId);
    void StopAllSounds();

    void SetMasterVolume(float v) { m_masterVolume = v; }
    float MasterVolume() const   { return m_masterVolume; }

    // Called from SDL audio callback
    void FillBuffer(int16_t* out, int numSamples);

private:
    struct Listener {
        Vec3 origin;
        Vec3 forward, right, up;
    } m_listener;

    std::unordered_map<std::string, int> m_nameToId;
    std::vector<SoundBuffer>             m_sounds;
    std::vector<SoundEmitter>            m_emitters;

    float m_masterVolume = 1.0f;
    bool  m_initialized  = false;
    uint32_t m_deviceId  = 0;

    static constexpr float ATTN_DIST_FULL = 512.0f;  // 100% volume within this distance
    static constexpr float ATTN_DIST_MAX  = 2048.0f; // silent beyond this

    void ComputeSpatial(const SoundEmitter& em,
                        float& outLeft, float& outRight) const;

    int AllocEmitter(int channel);
};

} // namespace OS
