#include "mavlink_handler.h"
#include <iostream>
#include <iomanip>
#include <signal.h>
#include <unistd.h>
#include <vector>

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = 0;
}


int main() {
    std::cout << "MAVLink Handler Example" << std::endl;
    std::cout << "======================" << std::endl;

    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Create MAVLink handler with System ID 1, Component ID 1
    MavlinkHandler mavlink(1, 1);
    
    if (!mavlink.initialize("/dev/ttyAMA0", 115200)) {
        std::cerr << "Failed to initialize UART" << std::endl;
        std::cerr << "Make sure:" << std::endl;
        std::cerr << "1. UART is enabled: sudo raspi-config -> Interface Options -> Serial" << std::endl;
        std::cerr << "2. User has permission: sudo usermod -a -G dialout $USER" << std::endl;
        std::cerr << "3. Device exists: ls -la /dev/tty*" << std::endl;
        return -1;
    }
    // Configure heartbeat parameters
    // MAV_TYPE_QUADROTOR = 2, MAV_AUTOPILOT_ARDUPILOTMEGA = 3
    mavlink.setHeartbeatParams(
        MAV_TYPE_QUADROTOR,           // Vehicle type: Quadrotor
        MAV_AUTOPILOT_ARDUPILOTMEGA,  // Autopilot: ArduPilot
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED | MAV_MODE_FLAG_STABILIZE_ENABLED,
        4,                            // Custom mode (e.g., GUIDED mode)
        MAV_STATE_ACTIVE              // System status: Active
    );

    std::cout << "\nSending heartbeat messages..." << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << "========================" << std::endl;
    mavlink.startHeartbeat(1000);
    // Main loop - keep the program running while heartbeat is active
    while (running && mavlink.isHeartbeatRunning()) {
        sleep(1);

        // You can add other MAVLink functionality here, such as:
        // - Processing incoming messages
        // - Sending other types of messages
        // - Handling commands
    }

    // Stop heartbeat
    mavlink.stopHeartbeat();

    std::cout << "MAVLink Handler Example finished" << std::endl;
    return 0;
}
