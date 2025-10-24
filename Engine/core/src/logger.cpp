#include "logger.hpp"
#include "asserts.hpp"

#include <iostream>
#include <memory> // For std::shared_ptr
#include <stdarg.h>
#include <string>
#include <vector> // For storing sinks

#include "spdlog/sinks/basic_file_sink.h"    // For basic file logging
#include "spdlog/sinks/stdout_color_sinks.h" // For console logging
#include "spdlog/spdlog.h"

#include "spdlog/spdlog.h"

void ReportAssertionFaliure(const char* expression, const char* message, const char* file, s32 line)
{
    LogOutput(LOG_LEVEL_FATAL, "Assertion failed: %s\nFile: %s\nLine: %d\nMessage: %s", expression, file, line, message);
}

b8 InitializeLogging(LogLevel minLevel, const char* logFilePath)
{
    try
    {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(spdlog::level::trace);

        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
        fileSink->set_level(spdlog::level::trace);

        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(consoleSink);
        sinks.push_back(fileSink);

        auto combinedLogger = std::make_shared<spdlog::logger>("engine_logger", begin(sinks), end(sinks));

        switch(minLevel)
        {
            case LOG_LEVEL_FATAL:
                combinedLogger->set_level(spdlog::level::critical);
                break;
            case LOG_LEVEL_ERROR:
                combinedLogger->set_level(spdlog::level::err);
                break;
            case LOG_LEVEL_WARN:
                combinedLogger->set_level(spdlog::level::warn);
                break;
            case LOG_LEVEL_INFO:
                combinedLogger->set_level(spdlog::level::info);
                break;
            case LOG_LEVEL_DEBUG:
                combinedLogger->set_level(spdlog::level::debug);
                break;
            case LOG_LEVEL_TRACE:
                combinedLogger->set_level(spdlog::level::trace);
                break;
            default:
                combinedLogger->set_level(spdlog::level::info);
                break;
        }

        combinedLogger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");

        spdlog::set_default_logger(combinedLogger);
        HGINFO("Logging initialized. Log file at: %s", logFilePath);
    }
    catch(const spdlog::spdlog_ex& ex)
    {
        // Use std::cerr or printf for logging initialization errors, as spdlog might not be ready
        // fprintf(stderr, "Log initialization failed: %s\n", ex.what());
        std::cerr << "Log init failed! Error: " << ex.what();
        return false;
    }
    return true;
}

void PauseLogging() { spdlog::set_level(spdlog::level::off); }
void ResumeLogging() { spdlog::set_level(spdlog::level::trace); }

void ShutDownLogging()
{
    HGINFO("Shutting down logging...");
    spdlog::shutdown();
    HGINFO("Shutdown logging");
}

void LogOutput(LogLevel level, const char* message, ...)
{
    std::string outMessage;
    outMessage.resize(32000);

    va_list argPtr;
    va_start(argPtr, message);
    u32 charsWritten = vsnprintf(&outMessage[0], outMessage.size(), message, argPtr);
    va_end(argPtr);

    if(charsWritten > 0 && static_cast<size_t>(charsWritten) < outMessage.size()) { outMessage.resize(charsWritten); }
    else if(charsWritten >= static_cast<u32>(outMessage.size()))
    {
        spdlog::warn("Log message truncated due to buffer size limit.");
        outMessage.back() = '\0';
    }
    else
    {
        spdlog::error("vsnprintf error during log formatting.");
        return;
    }

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
}
