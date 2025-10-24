#pragma once
#include "defines.hpp"

#define LOG_WARN_ENABLE 1
#define LOG_TRACE_ENABLE 0
#define LOG_DEBUG_ENABLE 1
#define LOG_INFO_ENABLE 1

#ifdef HGRELEASE
#define LOG_DEBUG_ENABLE 0
#define LOG_TRACE_ENABLE 0
#endif

enum LogLevel
{
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5
};

b8   InitializeLogging(LogLevel minLevel = LOG_LEVEL_TRACE, const char* logFilePath = "log.txt");
void ShutDownLogging();

void LogOutput(LogLevel level, const char* message, ...);
void PauseLogging();
void ResumeLogging();

#define HGFATAL(message, ...) LogOutput(LOG_LEVEL_FATAL, message __VA_OPT__(, __VA_ARGS__))

// FIXME: VMA leak logs overrides don't work.

#ifndef HGERROR
#define HGERROR(message, ...) LogOutput(LOG_LEVEL_ERROR, message __VA_OPT__(, __VA_ARGS__))
#define VMA_LEAK_LOG_FORMAT(format, ...) LogOutput(LOG_LEVEL_ERROR, "[VMA LEAK] " format __VA_OPT__(, __VA_ARGS__))
#endif

#if LOG_WARN_ENABLE == 1
#define HGWARN(message, ...) LogOutput(LOG_LEVEL_WARN, message __VA_OPT__(, __VA_ARGS__))
#else
#define HGWARN(message, ...)
#endif

#if LOG_INFO_ENABLE == 1
#define HGINFO(message, ...) LogOutput(LOG_LEVEL_INFO, message __VA_OPT__(, __VA_ARGS__))
#else
#define HGINFO(message, ...)
#endif

#if LOG_DEBUG_ENABLE == 1
#define HGDEBUG(message, ...) LogOutput(LOG_LEVEL_DEBUG, message __VA_OPT__(, __VA_ARGS__))
#else
#define HGDEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLE == 1
#define HGTRACE(message, ...) LogOutput(LOG_LEVEL_TRACE, message __VA_OPT__(, __VA_ARGS__))
#define VMA_DEBUG_LOG_FORMAT(format, ...) LogOutput(LOG_LEVEL_DEBUG, "[VMA DEBUG] " format __VA_OPT__(, __VA_ARGS__))
#else
#define HGTRACE(message, ...)
#define VMA_DEBUG_LOG_FORMAT(format, ...)
#endif

// Basic error checking macro for OpenAL calls
#ifndef AL_CHECK
#define AL_CHECK(expr)                                                                                                                             \
    do                                                                                                                                             \
    {                                                                                                                                              \
        expr;                                                                                                                                      \
        ALenum error = alGetError();                                                                                                               \
        if(error != AL_NO_ERROR) { HGERROR("OpenAL Error: %s AT %s : %i", alGetString(error), __FILE__, __LINE__); }                               \
    } while(false)
#endif

#ifndef ALC_CHECK
#define ALC_CHECK(device, expr)                                                                                                                    \
    do                                                                                                                                             \
    {                                                                                                                                              \
        expr;                                                                                                                                      \
        ALCenum error = alcGetError(device);                                                                                                       \
        if(error != ALC_NO_ERROR) { HGERROR("OpenALC Error: %s AT %s : %i", alGetString(error), __FILE__, __LINE__); }                             \
    } while(false)
#endif
