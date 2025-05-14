#pragma once

#include "al.h"
#include "defines.hpp"
#include <string>
namespace Humongous
{
class AudioSource
{
public:
    AudioSource(const std::string& filePath) { Load(filePath); };
    AudioSource() {};
    ~AudioSource() {};

    void Play(const bool& loop = false);
    void Load(const std::string& filePath);

private:
    static constexpr ALuint INVALID_BUFFER = 9999;

    b32    m_playing{false};
    ALuint m_alBuffer = INVALID_BUFFER;
    ALuint m_sourceID = INVALID_BUFFER;
};
} // namespace Humongous
