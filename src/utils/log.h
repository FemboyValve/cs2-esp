#pragma once

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"

class Logger {
public:
    Logger() = delete;

    static void Init()
    {
        quill::Backend::start();

        // Create console sink
        auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink");

        // Create file sink (this will write to "logs/app.log")
        auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>("child.log");

        // Create logger with both sinks
        logger = quill::Frontend::create_or_get_logger("root", { console_sink, file_sink });

        LOG_INFO(logger, "Log initialized.");
    }

    static void Destroy()
    {
        quill::Backend::stop();
    }

    static quill::Logger* GetLogger() { return logger; }

private:
    static quill::Logger* logger;
};


#define LOG_ENABLED
#ifdef LOG_ENABLED

#define LOG_INIT() Logger::Init()
#define LOG_DESTROY() Logger::Destroy()

#define CLOG_INFO(...)   QUILL_LOG_INFO(Logger::GetLogger(), __VA_ARGS__)
#define CLOG_WARN(...)   QUILL_LOG_WARNING(Logger::GetLogger(), __VA_ARGS__)
#define CLOG_ERROR(...)  QUILL_LOG_ERROR(Logger::GetLogger(), __VA_ARGS__)

#else

#define LOG_INIT()
#define LOG_DESTROY()

#define CLOG_INFO(...)
#define CLOG_WARN(...)
#define CLOG_ERROR(...)

#endif
