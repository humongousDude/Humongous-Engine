#pragma once

#include <AL/al.h>
#include <AL/alc.h>
#include "audio_source.hpp"
#include "singleton.hpp"

#include "glm/vec3.hpp"

// #include <AL/alext.h>

namespace Humongous
{
class AudioEngine : Singleton<AudioEngine>
{
public:
    static bool Init() { return Get().Internal_Init(); }
    static void Shutdown() { Get().Internal_Shutdown(); }

    static void UpdateListener(const glm::vec3& position, const glm::vec3& velocity, const glm::vec3& forward, const glm::vec3& up)
    {
        Get().Internal_UpdateListener(position, velocity, forward, up);
    }

    static void UpdateSources() { Get().Internal_UpdateSources(); };

    static void Play(AudioSourceComponent& src, const bool& loop = false);

private:
    ALCdevice*  m_device;
    ALCcontext* m_context;
    ALuint      m_alBuffer;

    bool Internal_Init();
    void Internal_Shutdown();

    void Internal_UpdateListener(const glm::vec3& position, const glm::vec3& velocity, const glm::vec3& forward, const glm::vec3& up);

    void Internal_UpdateSources();
};
} // namespace Humongous
