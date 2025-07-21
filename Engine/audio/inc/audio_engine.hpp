#pragma once

#include "audio_source.hpp"
#include "singleton.hpp"
#include <AL/al.h>
#include <AL/alc.h>
#include <Eigen/Dense>

// #include <AL/alext.h>

namespace Humongous
{
class AudioEngine : Singleton<AudioEngine>
{
public:
    static bool Init() { return Get().Internal_Init(); }
    static void Shutdown() { Get().Internal_Shutdown(); }

    static void UpdateListener(const Eigen::Vector3f& position, const Eigen::Vector3f& velocity, const Eigen::Vector3f& forward,
                               const Eigen::Vector3f& up)
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

    void Internal_UpdateListener(const Eigen::Vector3f& position, const Eigen::Vector3f& velocity, const Eigen::Vector3f& forward,
                                 const Eigen::Vector3f& up);

    void Internal_UpdateSources();
};
} // namespace Humongous
