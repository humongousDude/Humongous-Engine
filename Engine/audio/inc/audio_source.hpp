#pragma once

#include "al.h"
#include "defines.hpp"
#include "entity_component_system/components/entity_component.hpp"
#include <string>

namespace Humongous
{
struct AudioSourceComponent : public EntityComponent
{
public:
    AudioSourceComponent(const std::string& filePath);
    AudioSourceComponent() {};
    ~AudioSourceComponent() {};

    static constexpr ALuint INVALID_BUFFER = 9999;

    b32  IsPlaying() const { return m_playing; }
    void Pause() { m_playing = false; }
    void UnPause() { m_playing = true; }

    const ALuint& GetALBuffer() const { return m_alBuffer; }
    const ALuint& GetSourceID() const { return m_sourceID; }

    void SetSourceID(const ALuint& id) { m_sourceID = id; }
    void SetALBuffer(const ALuint& id) { m_alBuffer = id; }

private:
    b32    m_playing{false};
    ALuint m_alBuffer = INVALID_BUFFER;
    ALuint m_sourceID = INVALID_BUFFER;
};
} // namespace Humongous
