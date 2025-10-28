#ifndef LOGGER_H
#define LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

/**
 * @brief Logger wrapper class for spdlog
 * 
 * Provides a simple interface for logging throughout the application.
 * Supports both console and file output with automatic log rotation.
 */
class Logger {
public:
    /**
     * @brief Initialize the logger system
     * @param logFilePath Path to the log file (e.g., "logs/app.log")
     * @param maxFileSize Maximum size of each log file in bytes (default: 5MB)
     * @param maxFiles Maximum number of rotating log files (default: 3)
     */
    static void initialize(const std::string& logFilePath = "/home/pi5/shared_folder/aiPlatform/logs/aiplatform.log",
                          size_t maxFileSize = 1024 * 1024 * 5,  // 5MB
                          size_t maxFiles = 3);

    /**
     * @brief Set the logging level
     * @param level Log level: trace, debug, info, warn, error, critical
     */
    static void setLevel(spdlog::level::level_enum level);

    /**
     * @brief Shutdown the logger (call before application exit)
     */
    static void shutdown();

    /**
     * @brief Get the default logger instance
     */
    static std::shared_ptr<spdlog::logger> getLogger();

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

// Convenience macros for easy logging
#define LOG_TRACE(...)    if(Logger::getLogger()) Logger::getLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    if(Logger::getLogger()) Logger::getLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)     if(Logger::getLogger()) Logger::getLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     if(Logger::getLogger()) Logger::getLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    if(Logger::getLogger()) Logger::getLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) if(Logger::getLogger()) Logger::getLogger()->critical(__VA_ARGS__)

// Macros with file and line information for debugging
#define LOG_TRACE_LOC(...)    if(Logger::getLogger()) Logger::getLogger()->trace("[{}:{}] " __VA_ARGS__, __FILE__, __LINE__)
#define LOG_DEBUG_LOC(...)    if(Logger::getLogger()) Logger::getLogger()->debug("[{}:{}] " __VA_ARGS__, __FILE__, __LINE__)
#define LOG_ERROR_LOC(...)    if(Logger::getLogger()) Logger::getLogger()->error("[{}:{}] " __VA_ARGS__, __FILE__, __LINE__)

#endif // LOGGER_H
