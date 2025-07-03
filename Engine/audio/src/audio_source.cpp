#include "audio_source.hpp"
#include "logger.hpp"
#include "sndfile.h"
#include <cstring>
#include <vector>

namespace Humongous
{

AudioSourceComponent::AudioSourceComponent(const std::string& filePath)
{
    SF_INFO sfInfo;
    std::memset(&sfInfo, 0, sizeof(sfInfo));
    SNDFILE* sndFile = sf_open(filePath.c_str(), SFM_READ, &sfInfo);

    if(!sndFile)
    {
        HGERROR("Failed to open audio file: \"%s\". Libsndfile Error: %s", filePath.c_str(), sf_strerror(nullptr));
        return;
    }

    ALenum alFormat = 0;
    int    fileSubFormat = sfInfo.format & SF_FORMAT_SUBMASK;

    if(sfInfo.channels == 1) // Mono
    {
        if(fileSubFormat == SF_FORMAT_PCM_16) { alFormat = AL_FORMAT_MONO16; }
        else if(fileSubFormat == SF_FORMAT_PCM_U8) { alFormat = AL_FORMAT_MONO8; }
        else if(fileSubFormat == SF_FORMAT_PCM_S8) { alFormat = AL_FORMAT_MONO8; }
        else if(fileSubFormat == SF_FORMAT_FLOAT) { alFormat = AL_FORMAT_MONO16; }
    }
    else if(sfInfo.channels == 2)
    {
        if(fileSubFormat == SF_FORMAT_PCM_16) { alFormat = AL_FORMAT_STEREO16; }
        else if(fileSubFormat == SF_FORMAT_PCM_U8) { alFormat = AL_FORMAT_STEREO8; }
        else if(fileSubFormat == SF_FORMAT_PCM_S8) { alFormat = AL_FORMAT_STEREO8; }
        else if(fileSubFormat == SF_FORMAT_FLOAT) { alFormat = AL_FORMAT_STEREO16; }
    }

    if(alFormat == 0)
    {
        HGERROR("Unsupported audio format or channels in file: \"%s\". Channels: %d, Libsndfile Format: 0x%X (Subtype: 0x%X)", filePath.c_str(),
                sfInfo.channels, sfInfo.format, fileSubFormat);
        sf_close(sndFile);
        return;
    }

    // Read audio data
    sf_count_t        numFrames = sfInfo.frames;
    sf_count_t        itemsToRead = numFrames * sfInfo.channels;
    std::vector<char> pcmDataForAL;
    sf_count_t        framesRead = 0;

    if(fileSubFormat == SF_FORMAT_PCM_16)
    {
        std::vector<short> temp_buffer(itemsToRead);
        framesRead = sf_readf_short(sndFile, temp_buffer.data(), numFrames);
        if(framesRead > 0)
        {
            pcmDataForAL.resize(framesRead * sfInfo.channels * sizeof(short));
            std::memcpy(pcmDataForAL.data(), temp_buffer.data(), pcmDataForAL.size());
        }
    }
    else if(fileSubFormat == SF_FORMAT_PCM_U8)
    {
        pcmDataForAL.resize(itemsToRead * sizeof(unsigned char)); // sizeof(unsigned char) is 1
        sf_count_t bytes_read = sf_read_raw(sndFile, pcmDataForAL.data(), pcmDataForAL.size());
        if(bytes_read >= 0)
        {
            framesRead = bytes_read / (sfInfo.channels * sizeof(unsigned char));
            pcmDataForAL.resize(bytes_read);
        }
        else { framesRead = -1; }
    }
    else if(fileSubFormat == SF_FORMAT_PCM_S8)
    {
        std::vector<char> signed_temp_buffer(itemsToRead);
        sf_count_t        bytes_read = sf_read_raw(sndFile, signed_temp_buffer.data(), signed_temp_buffer.size());
        if(bytes_read >= 0)
        {
            framesRead = bytes_read / (sfInfo.channels * sizeof(char));
            signed_temp_buffer.resize(bytes_read);

            pcmDataForAL.resize(bytes_read);
            unsigned char* dest_ptr = reinterpret_cast<unsigned char*>(pcmDataForAL.data());
            for(size_t i = 0; i < signed_temp_buffer.size(); ++i)
            {
                dest_ptr[i] = static_cast<unsigned char>(static_cast<short>(signed_temp_buffer[i]) + 128);
            }
        }
        else { framesRead = -1; }
    }
    else if(fileSubFormat == SF_FORMAT_FLOAT)
    {
        std::vector<float> temp_buffer(itemsToRead);
        framesRead = sf_readf_float(sndFile, temp_buffer.data(), numFrames);
        if(framesRead > 0)
        {
            pcmDataForAL.resize(framesRead * sfInfo.channels * sizeof(float));
            std::memcpy(pcmDataForAL.data(), temp_buffer.data(), pcmDataForAL.size());
        }
    }

    if(framesRead <= 0 && numFrames > 0)
    {
        HGERROR("Error reading audio data from file: \"%s\". Frames read: %lld. Libsndfile error: %s", filePath.c_str(),
                static_cast<long long>(framesRead), sf_strerror(sndFile));
        sf_close(sndFile);
        return;
    }

    if(pcmDataForAL.empty() && numFrames > 0)
    {
        HGERROR("No data was read or processed into the final buffer from audio file: \"%s\"", filePath.c_str());
        sf_close(sndFile);
        return;
    }

    ALuint alBuffer = 0;
    AL_CHECK(alGenBuffers(1, &alBuffer));

    ALenum gen_error = alGetError();
    if(gen_error != AL_NO_ERROR || alBuffer == 0)
    {
        HGERROR("Failed to generate OpenAL buffer (alGenBuffers) for file: \"%s\". AL Error: %s (0x%x)", filePath.c_str(), alGetString(gen_error),
                gen_error);
        sf_close(sndFile);
        return;
    }

    AL_CHECK(alBufferData(alBuffer, alFormat, pcmDataForAL.data(), static_cast<ALsizei>(pcmDataForAL.size()), sfInfo.samplerate));

    ALenum buffer_data_error = alGetError();
    if(buffer_data_error != AL_NO_ERROR)
    {
        HGERROR("Failed to load data into OpenAL buffer (alBufferData) for file: \"%s\". AL Error: %s (0x%x)", filePath.c_str(),
                alGetString(buffer_data_error), buffer_data_error);
        AL_CHECK(alDeleteBuffers(1, &alBuffer));
        sf_close(sndFile);
        return;
    }

    if(sf_close(sndFile) != 0) { HGERROR("Failed to close audio file properly (data already loaded): \"%s\"", filePath.c_str()); }

    HGINFO("Successfully loaded audio file: \"%s\" (Buffer ID: %u, ALFormat: 0x%X, Rate: %dHz, Size: %zu bytes, Frames: %lld)", filePath.c_str(),
           alBuffer, alFormat, sfInfo.samplerate, pcmDataForAL.size(), static_cast<long long>(framesRead));

    m_alBuffer = alBuffer;
}

} // namespace Humongous
