#pragma once

#include "AL/al.h"
#include "AL/alc.h"
#include "singleton.hpp"

// #include <AL/alext.h>

namespace Humongous
{
class AudioEngine : Singleton<AudioEngine>
{
public:
    static bool Init() { return Get().Internal_Init(); }
    static void Shutdown() { Get().Internal_Shutdown(); }

private:
    ALCdevice*  m_device;
    ALCcontext* m_context;
    ALuint      m_alBuffer;

    bool Internal_Init();
    void Internal_Shutdown();
};
} // namespace Humongous
