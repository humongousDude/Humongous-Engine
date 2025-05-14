#include "audio_source.hpp"
#include "al.h"
#include "logger.hpp"
#include "sndfile.h"
#include <cstring>
#include <vector>
namespace Humongous
{
void AudioSource::Play(const bool& loop)
{
    if(m_sourceID == INVALID_BUFFER) { AL_CHECK(alGenSources(1, &m_sourceID)); }

    ALint state;
    alGetSourcei(m_sourceID, AL_SOURCE_STATE, &state);
    if(state == AL_PLAYING) { m_playing = true; }
    alGetSourcei(m_sourceID, AL_SOURCE_STATE, &state);
    if(state == AL_STOPPED || state == AL_PAUSED) { m_playing = false; }

    if(m_playing) { return; }

    ALenum error = alGetError(); // Check error more directly after alGenSources
    if(error != AL_NO_ERROR || m_sourceID == 0)
    {
        HGERROR("PlaySound: Failed to generate OpenAL source. AL Error: %s (0x%x)", alGetString(error), error);
        return;
    }

    n32 gain = 1;

    // Set basic source properties
    AL_CHECK(alSourcef(m_sourceID, AL_PITCH, 1.0f));
    AL_CHECK(alSourcef(m_sourceID, AL_GAIN, gain));
    AL_CHECK(alSource3f(m_sourceID, AL_POSITION, 0.0f, 0.0f, 0.0f)); // At origin
    AL_CHECK(alSource3f(m_sourceID, AL_VELOCITY, 0.0f, 0.0f, 0.0f));
    AL_CHECK(alSourcei(m_sourceID, AL_LOOPING, loop ? AL_TRUE : AL_FALSE));

    // Attach the buffer with the sound data to the source
    AL_CHECK(alSourcei(m_sourceID, AL_BUFFER, m_alBuffer));
    error = alGetError();
    if(error != AL_NO_ERROR)
    {
        HGERROR("PlaySound: Failed to attach buffer %u to source %u. AL Error: %s (0x%x)", m_alBuffer, m_sourceID, alGetString(error), error);
        AL_CHECK(alDeleteSources(1, &m_sourceID)); // Clean up the generated source
        return;
    }

    // Play the source
    AL_CHECK(alSourcePlay(m_sourceID));
    error = alGetError();
    if(error != AL_NO_ERROR)
    {
        HGERROR("PlaySound: Failed to play source %u. AL Error: %s (0x%x)", m_sourceID, alGetString(error), error);
        // Source might still be valid but in an error state or failed to start.
        // Depending on the error, we might still return the m_sourceID or clean it up.
        // For robustness, if play fails, we could consider the attempt failed.
        AL_CHECK(alDeleteSources(1, &m_sourceID));
        return;
    }
}

void AudioSource::Load(const std::string& filePath)
{
    SF_INFO sfInfo;
    std::memset(&sfInfo, 0, sizeof(sfInfo));
    SNDFILE* sndFile = sf_open(filePath.c_str(), SFM_READ, &sfInfo);

    if(!sndFile)
    {
        HGERROR("Failed to open audio file: \"%s\". Libsndfile Error: %s", filePath.c_str(), sf_strerror(nullptr));
        return;
    }

    // Determine OpenAL format based on file info
    ALenum alFormat = 0;
    int    fileSubFormat = sfInfo.format & SF_FORMAT_SUBMASK;

    if(sfInfo.channels == 1) // Mono
    {
        if(fileSubFormat == SF_FORMAT_PCM_16) { alFormat = AL_FORMAT_MONO16; }
        else if(fileSubFormat == SF_FORMAT_PCM_U8)
        {
            alFormat = AL_FORMAT_MONO8; // OpenAL expects unsigned 8-bit
        }
        else if(fileSubFormat == SF_FORMAT_PCM_S8)
        {
            alFormat = AL_FORMAT_MONO8; // Needs conversion from signed to unsigned
        }
        else if(fileSubFormat == SF_FORMAT_FLOAT) { alFormat = AL_FORMAT_MONO16; }
    }
    else if(sfInfo.channels == 2) // Stereo
    {
        if(fileSubFormat == SF_FORMAT_PCM_16) { alFormat = AL_FORMAT_STEREO16; }
        else if(fileSubFormat == SF_FORMAT_PCM_U8) { alFormat = AL_FORMAT_STEREO8; }
        else if(fileSubFormat == SF_FORMAT_PCM_S8)
        {
            alFormat = AL_FORMAT_STEREO8; // Needs conversion
        }
        else if(fileSubFormat == SF_FORMAT_FLOAT) { alFormat = AL_FORMAT_STEREO16; }
    }
    // You can add support for more channel counts (e.g., 4, 5.1, 7.1) using AL_EXT_MCFORMATS if needed.

    if(alFormat == 0)
    {
        HGERROR("Unsupported audio format or channels in file: \"%s\". Channels: %d, Libsndfile Format: 0x%X (Subtype: 0x%X)", filePath.c_str(),
                sfInfo.channels, sfInfo.format, fileSubFormat);
        sf_close(sndFile);
        return;
    }

    // Read audio data
    sf_count_t        numFrames = sfInfo.frames;
    sf_count_t        itemsToRead = numFrames * sfInfo.channels; // Total number of samples
    std::vector<char> pcmDataForAL;                              // This will hold the final data for alBufferData
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
    else if(fileSubFormat == SF_FORMAT_PCM_U8) // Unsigned 8-bit
    {
        pcmDataForAL.resize(itemsToRead * sizeof(unsigned char)); // sizeof(unsigned char) is 1
        sf_count_t bytes_read = sf_read_raw(sndFile, pcmDataForAL.data(), pcmDataForAL.size());
        if(bytes_read >= 0)
        {
            framesRead = bytes_read / (sfInfo.channels * sizeof(unsigned char));
            pcmDataForAL.resize(bytes_read); // Adjust to actual bytes read
        }
        else
        {
            framesRead = -1; // Indicate error from sf_read_raw
        }
    }
    else if(fileSubFormat == SF_FORMAT_PCM_S8) // Signed 8-bit, needs conversion to unsigned for OpenAL
    {
        std::vector<char> signed_temp_buffer(itemsToRead);
        sf_count_t        bytes_read = sf_read_raw(sndFile, signed_temp_buffer.data(), signed_temp_buffer.size());
        if(bytes_read >= 0)
        {
            framesRead = bytes_read / (sfInfo.channels * sizeof(char));
            signed_temp_buffer.resize(bytes_read); // Adjust to actual bytes read

            pcmDataForAL.resize(bytes_read);
            unsigned char* dest_ptr = reinterpret_cast<unsigned char*>(pcmDataForAL.data());
            for(size_t i = 0; i < signed_temp_buffer.size(); ++i)
            {
                // Convert signed char [-128, 127] to unsigned char [0, 255]
                dest_ptr[i] = static_cast<unsigned char>(static_cast<short>(signed_temp_buffer[i]) + 128);
            }
        }
        else
        {
            framesRead = -1; // Indicate error
        }
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
    // No else needed here, as unsupported formats are caught by alFormat == 0 check

    // Check for read errors or if no frames were read
    if(framesRead <= 0 && numFrames > 0)
    {
        HGERROR("Error reading audio data from file: \"%s\". Frames read: %lld. Libsndfile error: %s", filePath.c_str(),
                static_cast<long long>(framesRead), sf_strerror(sndFile));
        sf_close(sndFile);
        return;
    }

    // If frames_read < num_frames, it's a short read (EOF), which is usually acceptable.
    // pcm_data_for_al is already correctly sized based on actual frames/bytes read.

    if(pcmDataForAL.empty() && numFrames > 0)
    {
        HGERROR("No data was read or processed into the final buffer from audio file: \"%s\"", filePath.c_str());
        sf_close(sndFile);
        return;
    }
    // Create OpenAL buffer
    ALuint alBuffer = 0;
    AL_CHECK(alGenBuffers(1, &alBuffer)); // AL_CHECK will handle error logging if alGetError() is not AL_NO_ERROR

    ALenum gen_error = alGetError(); // Check error specifically after alGenBuffers, as AL_CHECK only checks after expr
    if(gen_error != AL_NO_ERROR || alBuffer == 0)
    {
        // AL_CHECK would have already logged, but this provides a clear failure point for this function.
        HGERROR("Failed to generate OpenAL buffer (alGenBuffers) for file: \"%s\". AL Error: %s (0x%x)", filePath.c_str(), alGetString(gen_error),
                gen_error);
        sf_close(sndFile);
        return; // Return 0 if buffer generation failed
    }

    // Upload audio data to OpenAL buffer
    AL_CHECK(alBufferData(alBuffer, alFormat, pcmDataForAL.data(), static_cast<ALsizei>(pcmDataForAL.size()), sfInfo.samplerate));

    ALenum buffer_data_error = alGetError(); // Check error specifically after alBufferData
    if(buffer_data_error != AL_NO_ERROR)
    {
        HGERROR("Failed to load data into OpenAL buffer (alBufferData) for file: \"%s\". AL Error: %s (0x%x)", filePath.c_str(),
                alGetString(buffer_data_error), buffer_data_error);
        AL_CHECK(alDeleteBuffers(1, &alBuffer)); // Clean up the generated buffer if data loading failed
        sf_close(sndFile);
        return;
    }

    // Close the audio file
    if(sf_close(sndFile) != 0)
    {
        HGERROR("Failed to close audio file properly (data already loaded): \"%s\"", filePath.c_str());
        // Not necessarily a fatal error for the loaded sound, but good to log.
    }

    HGINFO("Successfully loaded audio file: \"%s\" (Buffer ID: %u, ALFormat: 0x%X, Rate: %dHz, Size: %zu bytes, Frames: %lld)", filePath.c_str(),
           alBuffer, alFormat, sfInfo.samplerate, pcmDataForAL.size(), static_cast<long long>(framesRead));

    m_alBuffer = alBuffer;
}
} // namespace Humongous
