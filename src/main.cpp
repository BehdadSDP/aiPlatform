#include "include/application.h"
#include "include/logger.h"
#include <iostream>

int main(int argc, char *argv[]) {
    // Initialize logger first thing
    Logger::initialize("logs/aiplatform.log", 1024 * 1024 * 5, 3);  // 5MB per file, 3 files max
    
    // Set log level (can be changed via config later if needed)
    Logger::setLevel(spdlog::level::info);  // Options: trace, debug, info, warn, error, critical
    
    LOG_INFO("=== AI Platform Starting ===");
    
    try {
        std::string configPath = "/home/pi5/shared_folder/aiPlatform/config/config.txt";
        if (argc > 1) {
            configPath = argv[1];
            LOG_INFO("Using config file: {}", configPath);
        } else {
            LOG_INFO("Using default config file: {}", configPath);
        }

        Application app;
        if (!app.initialize(configPath)) {
            LOG_CRITICAL("Application failed to initialize");
            Logger::shutdown();
            return 1;
        }
        
        app.run();
        
        LOG_INFO("=== AI Platform Shutting Down ===");
        Logger::shutdown();
        return 0;

    } catch (const std::exception& e) {
        LOG_CRITICAL("FATAL ERROR: {}", e.what());
        Logger::shutdown();
        return 1;
    }
}
