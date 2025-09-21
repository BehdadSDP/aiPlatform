#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include <string>

// Serial communication includes
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

// MAVLink includes - using common dialect
#include "Output/common/mavlink.h"

/**
 * @brief MAVLink Handler class for sending heartbeat messages
 * 
 * This class provides functionality to send MAVLink heartbeat messages
 * using the common dialect from the generated MAVLink headers.
 */
class MavlinkHandler {
public:
    /**
     * @brief Constructor
     * 
     * @param system_id System ID for this MAVLink node (default: 1)
     * @param component_id Component ID for this MAVLink node (default: 1)
     */
    explicit MavlinkHandler(uint8_t system_id = 1, uint8_t component_id = 1);
    
    /**
     * @brief Destructor
     */
    ~MavlinkHandler();
    
    /**
     * @brief Initialize the MAVLink handler with serial communication
     * 
     * @param serial_device Path to serial device (default: "/dev/ttyAMA0")
     * @param baud_rate Baud rate for serial communication (default: 57600)
     * @return true if initialization successful, false otherwise
     */
    bool initialize(const std::string& serial_device = "/dev/ttyAMA10", int baud_rate = 57600);
    
    
    /**
     * @brief Start sending heartbeat messages periodically
     * 
     * @param interval_ms Heartbeat interval in milliseconds (default: 1000ms)
     * @return true if started successfully, false otherwise
     */
    bool startHeartbeat(uint32_t interval_ms = 1000);
    
    /**
     * @brief Stop sending heartbeat messages
     */
    void stopHeartbeat();
    
    /**
     * @brief Send a single heartbeat message
     * 
     * @return true if sent successfully, false otherwise
     */
    bool sendHeartbeat();
    
    
    /**
     * @brief Set heartbeat parameters
     * 
     * @param type Vehicle/component type (MAV_TYPE enum)
     * @param autopilot Autopilot type (MAV_AUTOPILOT enum)
     * @param base_mode System mode bitmap (MAV_MODE_FLAG enum)
     * @param custom_mode Custom mode for autopilot-specific flags
     * @param system_status System status (MAV_STATE enum)
     */
    void setHeartbeatParams(uint8_t type = MAV_TYPE_GENERIC,
                           uint8_t autopilot = MAV_AUTOPILOT_GENERIC,
                           uint8_t base_mode = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
                           uint32_t custom_mode = 0,
                           uint8_t system_status = MAV_STATE_ACTIVE);
    
    /**
     * @brief Get the current system ID
     */
    uint8_t getSystemId() const { return system_id_; }
    
    /**
     * @brief Get the current component ID
     */
    uint8_t getComponentId() const { return component_id_; }
    
    /**
     * @brief Check if heartbeat is currently running
     */
    bool isHeartbeatRunning() const { return heartbeat_running_; }
    
    /**
     * @brief Close serial connection
     */
    void closeSerial();

private:
    // System identification
    uint8_t system_id_;
    uint8_t component_id_;
    
    // Heartbeat parameters
    uint8_t heartbeat_type_;
    uint8_t heartbeat_autopilot_;
    uint8_t heartbeat_base_mode_;
    uint32_t heartbeat_custom_mode_;
    uint8_t heartbeat_system_status_;
    
    // Heartbeat control
    std::atomic<bool> heartbeat_running_;
    std::thread heartbeat_thread_;
    uint32_t heartbeat_interval_ms_;
    
    
    // Serial communication
    int serial_fd_;
    std::string serial_device_;
    bool use_serial_;
    
    /**
     * @brief Heartbeat thread function
     */
    void heartbeatThreadFunction();
    
    /**
     * @brief Create and send a MAVLink message
     * 
     * @param msg The MAVLink message to send
     * @return true if sent successfully, false otherwise
     */
    bool sendMessage(const mavlink_message_t& msg);
    
    /**
     * @brief Configure serial port settings
     * 
     * @param baud_rate Baud rate for serial communication
     * @return true if configuration successful, false otherwise
     */
    bool configureSerial(int baud_rate);
    
    /**
     * @brief Send data over serial port
     * 
     * @param buffer Data buffer to send
     * @param length Length of data to send
     * @return true if sent successfully, false otherwise
     */
    bool sendSerial(const uint8_t* buffer, uint16_t length);
};
