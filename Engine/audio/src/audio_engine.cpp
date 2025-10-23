#include "audio_engine.hpp"
#include "entity_component_system/components/transform_component.hpp"
#include "logger.hpp"
#include "scene_handler.hpp"

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

void AudioEngine::Internal_UpdateListener(const Eigen::Vector3f& position, const Eigen::Vector3f& velocity, const Eigen::Vector3f& forward,
                                          const Eigen::Vector3f& up)
{
    AL_CHECK(alListener3f(AL_POSITION, position.x(), position.y(), position.z()));
    AL_CHECK(alListener3f(AL_VELOCITY, velocity.x(), velocity.y(), velocity.z()));

    float orient[6] = {forward.x(), forward.y(), forward.z(), up.x(), up.y(), up.z()};
    AL_CHECK(alListenerfv(AL_ORIENTATION, orient));
}

void AudioEngine::Play(AudioSourceComponent& src, const bool& loop)
{
    ALuint srcID;
    if(src.GetSourceID() == AudioSourceComponent::INVALID_BUFFER)
    {
        AL_CHECK(alGenSources(1, &srcID));
        src.SetSourceID(srcID);
    }

    ALint state;
    alGetSourcei(src.GetSourceID(), AL_SOURCE_STATE, &state);
    if(state == AL_PLAYING) { src.UnPause(); }
    alGetSourcei(src.GetSourceID(), AL_SOURCE_STATE, &state);
    if(state == AL_STOPPED || state == AL_PAUSED) { src.Pause(); }

    // FIXME: this doesn't loop, will fix in later audio overhaul
    if(src.IsPlaying() && !loop) { return; }

    ALenum error = alGetError();
    if(error != AL_NO_ERROR || src.GetSourceID() == 0)
    {
        HGERROR("PlaySound: Failed to generate OpenAL source. AL Error: %s (0x%x)", alGetString(error), error);
        return;
    }

    AL_CHECK(alSourcei(src.GetSourceID(), AL_BUFFER, src.GetALBuffer()));
    error = alGetError();
    if(error != AL_NO_ERROR)
    {
        HGERROR("PlaySound: Failed to attach buffer %u to source %u. AL Error: %s (0x%x)", src.GetALBuffer(), src.GetSourceID(), alGetString(error),
                error);
        AL_CHECK(alDeleteSources(1, &src.GetSourceID())); // Clean up the generated source
        return;
    }

    AL_CHECK(alSourcePlay(src.GetSourceID()));
    error = alGetError();
    if(error != AL_NO_ERROR)
    {
        HGERROR("PlaySound: Failed to play source %u. AL Error: %s (0x%x)", src.GetSourceID(), alGetString(error), error);
        AL_CHECK(alDeleteSources(1, &src.GetSourceID()));
        return;
    }
}

void AudioEngine::Internal_UpdateSources()
{
    auto world = SceneHandler::GetWorld();
    for(const auto& entityId: world->GetComponentStorage<AudioSourceComponent>().GetDense())
    {
        auto audio = world->GetComponent<AudioSourceComponent>(entityId);
        auto transform = world->GetComponent<TransformComponent>(entityId);

        auto translation = transform->GetTranslation();

        if(audio->GetSourceID() != AudioSourceComponent::INVALID_BUFFER)
        {
            AL_CHECK(alSource3f(audio->GetSourceID(), AL_POSITION, translation.x(), translation.y(), translation.z()));
            AL_CHECK(alSource3f(audio->GetSourceID(), AL_VELOCITY, 1.1f, 0.5f, 8.0f));
        }
    }
}

} // namespace Humongous
