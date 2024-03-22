#include "../inc/logger.hpp"
#include "../inc/asserts.hpp"

// TODO: temp
#include <cstdio>
#include <stdarg.h>
#include <string>

// TODO: create log file
// TODO: Actually setup Boost.log, or maybe switch to a custom logging system

#include <boost/log/trivial.hpp>

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
    // b8 isError = level == LOG_LEVEL_ERROR || level == LOG_LEVEL_FATAL;

    std::string outMessage;
    outMessage.resize(320000);
    __builtin_va_list argPtr;
    va_start(argPtr, message);
    vsnprintf(&outMessage[0], outMessage.size(), message, argPtr);
    va_end(argPtr);

    switch(level)
    {
        case LOG_LEVEL_FATAL:
            BOOST_LOG_TRIVIAL(fatal) << outMessage;
            break;

        case LOG_LEVEL_ERROR:
            BOOST_LOG_TRIVIAL(error) << outMessage;
            break;

        case LOG_LEVEL_WARN:
            BOOST_LOG_TRIVIAL(warning) << outMessage;
            break;

        case LOG_LEVEL_INFO:
            BOOST_LOG_TRIVIAL(info) << outMessage;
            break;

        case LOG_LEVEL_DEBUG:
            BOOST_LOG_TRIVIAL(debug) << outMessage;
            break;

        case LOG_LEVEL_TRACE:
            BOOST_LOG_TRIVIAL(trace) << outMessage;
            break;
    }

    // TODO: platform specific output
}
