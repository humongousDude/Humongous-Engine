#include "../inc/logger.hpp"
#include "../inc/asserts.hpp"

// TODO: temp
#include <cstdio>
#include <fmt/core.h>
#include <stdarg.h>
#include <string>

// TODO: create log file
// TODO: make async

void ReportAssertionFaliure(const char* expression, const char* message, const char* file, i32 line)
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
    std::string LevelStrings[6] = {"[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: "};

    b8 isError = level == LOG_LEVEL_ERROR || level == LOG_LEVEL_FATAL;

    std::string outMessage;
    outMessage.resize(320000);
    __builtin_va_list argPtr;
    va_start(argPtr, message);
    vsnprintf(&outMessage[0], outMessage.size(), message, argPtr);
    va_end(argPtr);

    outMessage.insert(0, LevelStrings[level]);
    outMessage.append("\n");

    // TODO: platform specific output
    fmt::print(stderr, "{}", outMessage);
}
