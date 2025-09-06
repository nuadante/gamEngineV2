#pragma once

#include <string>
#include <unordered_map>
#include <vector>

typedef unsigned int ALuint;

namespace engine
{
    struct Sound
    {
        ALuint buffer = 0;
        int channels = 0;
        int sampleRate = 0;
    };

    class AudioEngine
    {
    public:
        bool initialize();
        void shutdown();

        // Load WAV/OGG into buffer
        ALuint loadSound(const std::string& path);

        // Play 2D/3D
        ALuint play(ALuint buffer, float gain = 1.0f, bool loop = false);
        ALuint play3D(ALuint buffer, float x, float y, float z, float gain = 1.0f, bool loop = false);
        void stop(ALuint source);

        // Listener
        void setListener(float x, float y, float z, float fx, float fy, float fz, float ux, float uy, float uz);

        void update();

    private:
        void* m_device = nullptr;
        void* m_context = nullptr;
        std::unordered_map<std::string, ALuint> m_cache;
        std::vector<ALuint> m_sources; // for cleanup
    };
}


