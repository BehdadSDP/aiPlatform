#include "include/application.h"
#include "include/mainwindow.h"
#include "include/logger.h"
#include <QApplication>
#include <iostream>
#include <cstdlib>

int main(int argc, char *argv[]) {
    // Set environment variables to avoid GTK conflicts on Raspberry Pi
    qputenv("NO_AT_BRIDGE", "1");
    qputenv("QT_QPA_PLATFORMTHEME", "");
    
    // Initialize logger first thing
    Logger::initialize("logs/aiplatform.log", 1024 * 1024 * 5, 3);  // 5MB per file, 3 files max
    
    // Set log level (can be changed via config later if needed)
    Logger::setLevel(spdlog::level::info);  // Options: trace, debug, info, warn, error, critical
    
    LOG_INFO("=== AI Platform Starting ===");
    
    try {
        // Check if GUI mode is requested (default)
        bool useGUI = true;
        std::string configPath = "/home/pi5/shared_folder/aiPlatform/config/config.txt";
            
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--no-gui" || arg == "-ng") {
                useGUI = false;
            } else if (arg == "--config" || arg == "-c") {
                if (i + 1 < argc) {
                    configPath = argv[++i];
                    LOG_INFO("Using config file: {}", configPath);
                }
            } else {
                configPath = argv[i];
                LOG_INFO("Using config file: {}", configPath);
            }
        }
        
        if (useGUI) {
            // GUI Mode
            LOG_INFO("Starting with GUI mode");
            QApplication qapp(argc, argv);
            
            MainWindow window;
            window.show();
            
            int result = qapp.exec();
            
            LOG_INFO("=== AI Platform Shutting Down ===");
            Logger::shutdown();
            return result;
        } else {
            // Console Mode (original behavior)
            LOG_INFO("Starting with console mode");
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
        }

    } catch (const std::exception& e) {
        LOG_CRITICAL("FATAL ERROR: {}", e.what());
        Logger::shutdown();
        return 1;
    }
}