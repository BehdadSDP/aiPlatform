#include "include/application.h"
#include <iostream>

int main(int argc, char *argv[]) {
    try {
        std::string configPath = "/home/pi5/shared_folder/aiPlatform/config/config.txt";
        if (argc > 1) {
            configPath = argv[1];
        }

        Application app;
        if (!app.initialize(configPath)) {
            std::cerr << "Application failed to initialize." << std::endl;
            return 1;
        }

        app.run();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
}
