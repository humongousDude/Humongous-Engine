#include "logger.hpp"
#include "asserts.hpp"

#include <stdarg.h>
#include <string>

#include <memory> // For std::shared_ptr
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
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
        file_sink->set_level(spdlog::level::trace);

        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(console_sink);
        sinks.push_back(file_sink);

        auto combined_logger = std::make_shared<spdlog::logger>("engine_logger", begin(sinks), end(sinks));

        switch(minLevel)
        {
            case LOG_LEVEL_FATAL:
                combined_logger->set_level(spdlog::level::critical);
                break;
            case LOG_LEVEL_ERROR:
                combined_logger->set_level(spdlog::level::err);
                break;
            case LOG_LEVEL_WARN:
                combined_logger->set_level(spdlog::level::warn);
                break;
            case LOG_LEVEL_INFO:
                combined_logger->set_level(spdlog::level::info);
                break;
            case LOG_LEVEL_DEBUG:
                combined_logger->set_level(spdlog::level::debug);
                break;
            case LOG_LEVEL_TRACE:
                combined_logger->set_level(spdlog::level::trace);
                break;
            default:
                combined_logger->set_level(spdlog::level::info);
                break;
        }

        combined_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");

        spdlog::set_default_logger(combined_logger);
        HGINFO("Logging initialized. Log file at: %s", logFilePath);
    }
    catch(const spdlog::spdlog_ex& ex)
    {
        // Use std::cerr or printf for logging initialization errors, as spdlog might not be ready
        fprintf(stderr, "Log initialization failed: %s\n", ex.what());
        return false;
    }
    return true;
}

void ShutDownLogging()
{
    HGINFO("Shutting down logging...");
    spdlog::shutdown();
    HGINFO("Shutdown logging");
}

void LogOutput(LogLevel level, const char* message, ...)
{
    std::string outMessage;
    outMessage.resize(64000);

    va_list argPtr;
    va_start(argPtr, message);
    int chars_written = vsnprintf(&outMessage[0], outMessage.size(), message, argPtr);
    va_end(argPtr);

    if(chars_written > 0 && static_cast<size_t>(chars_written) < outMessage.size())
    {
        outMessage.resize(chars_written); // Resize to actual content length
    }
    else if(chars_written >= static_cast<int>(outMessage.size()))
    {
        spdlog::warn("Log message truncated due to buffer size limit.");
        outMessage.back() = '\0';
    }
    else
    {
        // vsnprintf error
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
