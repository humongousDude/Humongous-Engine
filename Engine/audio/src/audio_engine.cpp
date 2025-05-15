#include "audio_engine.hpp"
#include "logger.hpp"

namespace Humongous
{
bool AudioEngine::Internal_Init()
{
    m_device = alcOpenDevice(nullptr);
    if(!m_device)
    {
        HGERROR("Failed to open default OpenAL device.");
        return false;
    }

    HGINFO("Opened OpenAL device: %s", static_cast<const char*>(alcGetString(m_device, ALC_DEVICE_SPECIFIER)));

    if(alcIsExtensionPresent(m_device, "ALC_SOFT_HRTF")) { HGINFO("HRTF extension available!"); }

    m_context = alcCreateContext(m_device, nullptr);
    if(!m_context)
    {
        HGERROR("Failed to create OpenAL m_context.");
        alcCloseDevice(m_device);
        m_device = nullptr;
        return false;
    }

    if(!alcMakeContextCurrent(m_context))
    {
        HGERROR("Failed to make OpenAL m_context current.");
        alcDestroyContext(m_context);
        m_context = nullptr;
        alcCloseDevice(m_device);
        m_device = nullptr;
        return false;
    }
    ALC_CHECK(m_device, );

    HGINFO("OpenAL m_context created and made current.");
    HGINFO("OpenAL Vendor: %s", static_cast<const char*>(alGetString(AL_VENDOR)));
    HGINFO("OpenAL Version: %s", static_cast<const char*>(alGetString(AL_VERSION)));
    HGINFO("OpenAL Renderer: %s", static_cast<const char*>(alGetString(AL_RENDERER)));

    AL_CHECK(alListener3f(AL_POSITION, 5.0f, 4.0f, 0.0f));
    AL_CHECK(alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f));
    float listenerOrientation[] = {0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f};
    AL_CHECK(alListenerfv(AL_ORIENTATION, listenerOrientation));

    return true;
}

void AudioEngine::Internal_Shutdown()
{
    if(m_context)
    {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(m_context);
        m_context = nullptr;
        HGINFO("OpenAL context destroyed.");
    }
    if(m_device)
    {
        alcCloseDevice(m_device);
        m_device = nullptr;
        HGINFO("OpenAL device closed.");
    }
}

void AudioEngine::Internal_UpdateListener(const glm::vec3& position, const glm::vec3& velocity, const glm::vec3& forward, const glm::vec3& up)
{
    AL_CHECK(alListener3f(AL_POSITION, position.x, position.y, position.z));
    AL_CHECK(alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z));

    float orient[6] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
    AL_CHECK(alListenerfv(AL_ORIENTATION, orient));
}

} // namespace Humongous
