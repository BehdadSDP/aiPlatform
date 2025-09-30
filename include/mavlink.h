#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>

// Serial communication includes
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

// MAVLink includes - using common dialect for MAVLink2
#include "Output/common/mavlink.h"

/**
 * @brief Enhanced MAVLink2 class for comprehensive flight controller communication
 * 
 * This class provides full MAVLink2 protocol support for communication with
 * flight controllers including command sending, parameter management, and
 * mission control capabilities optimized for Raspberry Pi 5.
 */
class Mavlink {
public:
    /**
     * @brief Constructor
     * 
     * @param system_id System ID for this MAVLink node (default: 1)
     * @param component_id Component ID for this MAVLink node (default: 1)
     */
    Mavlink(uint8_t system_id = 1, uint8_t component_id = 1);
    
    /**
     * @brief Destructor
     */
    ~Mavlink();
    
    // === INITIALIZATION ===
    
    /**
     * @brief Initialize UART communication on Raspberry Pi 5
     * 
     * @param serial_device Path to serial device (default: "/dev/ttyAMA0")
     * @param baud_rate Baud rate for serial communication (default: 57600)
     * @return true if initialization successful, false otherwise
     */
    bool initializeUART(const std::string& serial_device = "/dev/ttyAMA0", int baud_rate = 57600);
    
    /**
     * @brief Start the MAVLink communication system
     * @return true if started successfully, false otherwise
     */
    bool start();
    
    /**
     * @brief Stop the MAVLink communication system
     */
    void stop();
    
    // === HEARTBEAT MANAGEMENT ===
    
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
     * @brief Set heartbeat parameters
     * 
     * @param type Vehicle/component type (MAV_TYPE enum)
     * @param autopilot Autopilot type (MAV_AUTOPILOT enum)
     * @param base_mode System mode bitmap (MAV_MODE_FLAG enum)
     * @param custom_mode Custom mode for autopilot-specific flags
     * @param system_status System status (MAV_STATE enum)
     */
    void setHeartbeatParams(uint8_t type = MAV_TYPE_GCS,
                           uint8_t autopilot = MAV_AUTOPILOT_GENERIC,
                           uint8_t base_mode = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
                           uint32_t custom_mode = 0,
                           uint8_t system_status = MAV_STATE_ACTIVE);
    
    // === FLIGHT CONTROLLER COMMANDS ===
    
    /**
     * @brief Arm/Disarm the vehicle
     * 
     * @param arm true to arm, false to disarm
     * @param force Force arm/disarm even if safety checks fail
     * @return true if command sent successfully
     */
    bool armDisarm(bool arm, bool force = false);
    
    /**
     * @brief Set flight mode
     * 
     * @param mode Flight mode (depends on autopilot type)
     * @return true if command sent successfully
     */
    bool setFlightMode(uint32_t mode);
    
    /**
     * @brief Request takeoff
     * 
     * @param altitude Target altitude in meters
     * @param latitude Target latitude (optional, 0 for current position)
     * @param longitude Target longitude (optional, 0 for current position)
     * @return true if command sent successfully
     */
    bool takeoff(float altitude, double latitude = 0.0, double longitude = 0.0);
    
    /**
     * @brief Request landing
     * 
     * @param latitude Target latitude (optional, 0 for current position)
     * @param longitude Target longitude (optional, 0 for current position)
     * @return true if command sent successfully
     */
    bool land(double latitude = 0.0, double longitude = 0.0);
    
    /**
     * @brief Check if MAVLink is properly connected and ready
     * 
     * @return true if MAVLink is initialized and running
     */
    bool isConnected() const { return uart_initialized_ && running_; }
    
    /**
     * @brief Test MAVLink message format to verify proper encoding
     */
    void testMAVLinkMessageFormat();
    

private:
    // === CORE VARIABLES ===
    std::mutex uart_mutex_;
    // System identification
    uint8_t system_id_;
    uint8_t component_id_;
    uint8_t target_system_id_;
    
    // Heartbeat parameters
    uint8_t heartbeat_type_;
    uint8_t heartbeat_autopilot_;
    uint8_t heartbeat_base_mode_;
    uint32_t heartbeat_custom_mode_;
    uint8_t heartbeat_system_status_;
    
    // Communication control
    std::atomic<bool> running_;
    std::atomic<bool> heartbeat_running_;
    uint32_t heartbeat_interval_ms_;
    
    // Threading
    std::thread heartbeat_thread_;
    
    // UART communication
    int serial_fd_;
    std::string serial_device_;
    int baud_rate_;
    bool uart_initialized_;
    
    // === PRIVATE METHODS ===
    
    /**
     * @brief Heartbeat thread function
     */
    void heartbeatThreadFunction();
    
    /**
     * @brief Send a MAVLink message immediately
     * 
     * @param msg The MAVLink message to send
     * @return true if sent successfully, false otherwise
     */
    bool sendMessageImmediate(const mavlink_message_t& msg);
    
    /**
     * @brief Configure UART port settings for Raspberry Pi 5
     * 
     * @param baud_rate Baud rate for serial communication
     * @return true if configuration successful, false otherwise
     */
    bool configureUART(int baud_rate);
    
    /**
     * @brief Send data over UART port
     * 
     * @param buffer Data buffer to send
     * @param length Length of data to send
     * @return true if sent successfully, false otherwise
     */
    bool sendUART(const uint8_t* buffer, uint16_t length);
    
    /**
     * @brief Send a command_long message
     * 
     * @param command MAV_CMD command ID
     * @param param1-7 Command parameters
     * @return true if sent successfully
     */
    bool sendCommandLong(uint16_t command, float param1 = 0.0f, float param2 = 0.0f, 
                        float param3 = 0.0f, float param4 = 0.0f, float param5 = 0.0f, 
                        float param6 = 0.0f, float param7 = 0.0f);
};
