#include "logger.hpp"
#include "asserts.hpp"

#include <stdarg.h>
#include <string>

// TODO: create log file

#include "spdlog/spdlog.h"

void ReportAssertionFaliure(const char* expression, const char* message, const char* file, s32 line)
{
    LogOutput(LOG_LEVEL_FATAL, "Assertion failed: %s\nFile: %s\nLine: %d\nMessage: %s", expression, file, line, message);
}

b8 InitializeLogging(LogLevel level)
{
    // TODO: create log file
    return true;
}

void ShutDownLogging()
{
    // TODO: cleanup loggind/write queued entries
}

void LogOutput(LogLevel level, const char* message, ...)
{
    std::string outMessage;
    outMessage.resize(320000);
    __builtin_va_list argPtr;
    va_start(argPtr, message);
    vsnprintf(&outMessage[0], outMessage.size(), message, argPtr);
    va_end(argPtr);

    switch(level)
    {
        case LOG_LEVEL_FATAL:
            spdlog::critical(outMessage);
            break;

        case LOG_LEVEL_ERROR:
            spdlog::error(outMessage);
            break;

        case LOG_LEVEL_WARN:
            spdlog::warn(outMessage);
            break;

        case LOG_LEVEL_INFO:
            spdlog::info(outMessage);
            break;

        case LOG_LEVEL_DEBUG:
            spdlog::debug(outMessage);
            break;

        case LOG_LEVEL_TRACE:
            spdlog::trace(outMessage);
            break;
    }

    // TODO: platform specific output
}
