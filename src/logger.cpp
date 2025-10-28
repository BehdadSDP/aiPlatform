#include "include/logger.h"
#include <iostream>
#include <vector>
#include <filesystem>

// Initialize static member
std::shared_ptr<spdlog::logger> Logger::s_logger = nullptr;

void Logger::initialize(const std::string& logFilePath, size_t maxFileSize, size_t maxFiles) {
    try {
        // Create logs directory if it doesn't exist
        std::filesystem::path logPath(logFilePath);
        std::filesystem::path logDir = logPath.parent_path();
        
        if (!logDir.empty() && !std::filesystem::exists(logDir)) {
            std::filesystem::create_directories(logDir);
        }

        // Create sinks (outputs)
        std::vector<spdlog::sink_ptr> sinks;
        
        // Console sink with colors
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        sinks.push_back(console_sink);
        
        // Rotating file sink (automatically rotates when file reaches max size)
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFilePath, maxFileSize, maxFiles);
        file_sink->set_level(spdlog::level::trace);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
        sinks.push_back(file_sink);
        
        // Create logger with both sinks
        s_logger = std::make_shared<spdlog::logger>("aiplatform", sinks.begin(), sinks.end());
        s_logger->set_level(spdlog::level::info);  // Default level
        s_logger->flush_on(spdlog::level::warn);   // Auto-flush on warnings and above
        
        // Register as default logger
        spdlog::set_default_logger(s_logger);
        
        LOG_INFO("Logger initialized successfully");
        LOG_INFO("Log file: {}", logFilePath);
        
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
    }
}

void Logger::setLevel(spdlog::level::level_enum level) {
    if (s_logger) {
        s_logger->set_level(level);
        LOG_INFO("Log level set to: {}", spdlog::level::to_string_view(level));
    }
}

void Logger::shutdown() {
    if (s_logger) {
        LOG_INFO("Logger shutting down");
        s_logger->flush();
        s_logger.reset();
    }
    spdlog::shutdown();
}

std::shared_ptr<spdlog::logger> Logger::getLogger() {
    return s_logger;
}
