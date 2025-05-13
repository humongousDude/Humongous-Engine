#pragma once

#include "AL/al.h"
#include "AL/alc.h"
#include "singleton.hpp"
#include "thread"

// #include <AL/alext.h>

namespace Humongous
{
class AudioEngine : Singleton<AudioEngine>
{
public:
    static bool Init() { return Get().Internal_Init(); }
    static void Shutdown() { Get().Internal_Shutdown(); }
    static void PlaySound(class AudioSource& audioSource) { Get().Internal_PlaySound(audioSource); }

private:
    ALCdevice*   m_device;
    ALCcontext*  m_context;
    ALuint       m_alBuffer;
    std::jthread m_audioThread;

    bool Internal_Init();
    void Internal_Shutdown();

    void Internal_PlaySound(class AudioSource& audioSource);
};
} // namespace Humongous
