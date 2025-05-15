#pragma once

#include "AL/al.h"
#include "AL/alc.h"
#include "glm/glm.hpp"
#include "singleton.hpp"

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

private:
    ALCdevice*  m_device;
    ALCcontext* m_context;
    ALuint      m_alBuffer;

    bool Internal_Init();
    void Internal_Shutdown();

    void Internal_UpdateListener(const glm::vec3& position, const glm::vec3& velocity, const glm::vec3& forward, const glm::vec3& up);
};
} // namespace Humongous
