#include "mavlink_handler.h"
#include <iostream>
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <cstdio>

MavlinkHandler::MavlinkHandler(uint8_t system_id, uint8_t component_id)
    : system_id_(system_id)
    , component_id_(component_id)
    , heartbeat_type_(MAV_TYPE_GENERIC)
    , heartbeat_autopilot_(MAV_AUTOPILOT_GENERIC)
    , heartbeat_base_mode_(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED)
    , heartbeat_custom_mode_(0)
    , heartbeat_system_status_(MAV_STATE_ACTIVE)
    , heartbeat_running_(false)
    , heartbeat_interval_ms_(1000)
    , serial_fd_(-1)
    , serial_device_("")
    , use_serial_(false)
{
}

MavlinkHandler::~MavlinkHandler()
{
    stopHeartbeat();
    closeSerial();
}

bool MavlinkHandler::initialize(const std::string& serial_device, int baud_rate)
{
    // Store serial device settings
    serial_device_ = serial_device;
    use_serial_ = true;
    
    // Check if device exists first
    if (access(serial_device.c_str(), F_OK) != 0) {
        std::cerr << "Serial device " << serial_device << " does not exist" << std::endl;
        return false;
    }
    
    // Check if we have permission to access the device
    if (access(serial_device.c_str(), R_OK | W_OK) != 0) {
        std::cerr << "No permission to access " << serial_device 
                  << ". Try: sudo usermod -a -G dialout $USER && sudo chmod 666 " 
                  << serial_device << std::endl;
        return false;
    }
    
    // Open serial port
    serial_fd_ = open(serial_device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd_ < 0) {
        std::cerr << "Error opening serial device " << serial_device 
                  << ": " << strerror(errno) << " (errno: " << errno << ")" << std::endl;
        std::cerr << "Common solutions:" << std::endl;
        std::cerr << "  1. Enable UART: sudo raspi-config -> Interface Options -> Serial" << std::endl;
        std::cerr << "  2. Add user to dialout group: sudo usermod -a -G dialout $USER" << std::endl;
        std::cerr << "  3. Check device exists: ls -la /dev/serial* /dev/tty*" << std::endl;
        return false;
    }
    
    // Configure serial port
    if (!configureSerial(baud_rate)) {
        close(serial_fd_);
        serial_fd_ = -1;
        return false;
    }
    
    // Initialize MAVLink system
    mavlink_system_t mavlink_system = {
        .sysid = system_id_,
        .compid = component_id_
    };
    
    std::cout << "MAVLink Handler initialized with System ID: " 
              << static_cast<int>(system_id_) 
              << ", Component ID: " 
              << static_cast<int>(component_id_) 
              << std::endl;
    std::cout << "Serial device: " << serial_device 
              << ", Baud rate: " << baud_rate << std::endl;
    
    return true;
}


bool MavlinkHandler::startHeartbeat(uint32_t interval_ms)
{
    if (heartbeat_running_) {
        std::cout << "Heartbeat is already running" << std::endl;
        return false;
    }
    
    if (!use_serial_) {
        std::cout << "Serial not configured. Cannot start heartbeat." << std::endl;
        return false;
    }
    
    heartbeat_interval_ms_ = interval_ms;
    heartbeat_running_ = true;
    
    // Start heartbeat thread
    heartbeat_thread_ = std::thread(&MavlinkHandler::heartbeatThreadFunction, this);
    
    std::cout << "Heartbeat started with interval: " << interval_ms << "ms" << std::endl;
    return true;
}

void MavlinkHandler::stopHeartbeat()
{
    if (heartbeat_running_) {
        heartbeat_running_ = false;
        
        if (heartbeat_thread_.joinable()) {
            heartbeat_thread_.join();
        }
        
        std::cout << "Heartbeat stopped" << std::endl;
    }
}

bool MavlinkHandler::sendHeartbeat()
{
    mavlink_message_t msg;
    
    // Pack heartbeat message
    mavlink_msg_heartbeat_pack(
        system_id_,                    // system_id
        component_id_,                 // component_id
        &msg,                          // message
        heartbeat_type_,               // type
        heartbeat_autopilot_,          // autopilot
        heartbeat_base_mode_,          // base_mode
        heartbeat_custom_mode_,        // custom_mode
        heartbeat_system_status_       // system_status
    );
    
    return sendMessage(msg);
}


void MavlinkHandler::setHeartbeatParams(uint8_t type, uint8_t autopilot, uint8_t base_mode, 
                                       uint32_t custom_mode, uint8_t system_status)
{
    heartbeat_type_ = type;
    heartbeat_autopilot_ = autopilot;
    heartbeat_base_mode_ = base_mode;
    heartbeat_custom_mode_ = custom_mode;
    heartbeat_system_status_ = system_status;
    
    std::cout << "Heartbeat parameters updated - Type: " << static_cast<int>(type)
              << ", Autopilot: " << static_cast<int>(autopilot)
              << ", Mode: " << static_cast<int>(base_mode)
              << ", Custom: " << custom_mode
              << ", Status: " << static_cast<int>(system_status) << std::endl;
}

void MavlinkHandler::heartbeatThreadFunction()
{
    while (heartbeat_running_) {
        // Send heartbeat
        if (!sendHeartbeat()) {
            std::cerr << "Failed to send heartbeat" << std::endl;
        }
        
        // Sleep for the specified interval
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval_ms_));
    }
}

bool MavlinkHandler::sendMessage(const mavlink_message_t& msg)
{
    // Convert MAVLink message to buffer
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    uint16_t length = mavlink_msg_to_send_buffer(buffer, &msg);
    
    // Send via serial port
    bool result = sendSerial(buffer, length);
    
    if (result) {
        std::cout << "Sent MAVLink message ID: " << msg.msgid 
                  << ", Length: " << length << " bytes" << std::endl;
    } else {
        std::cerr << "Failed to send MAVLink message ID: " << msg.msgid << std::endl;
    }
    
    return result;
}

bool MavlinkHandler::configureSerial(int baud_rate)
{
    struct termios tty;
    
    if (tcgetattr(serial_fd_, &tty) != 0) {
        std::cerr << "Error getting serial attributes: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set baud rate
    speed_t baud;
    switch (baud_rate) {
        case 9600:   baud = B9600;   break;
        case 19200:  baud = B19200;  break;
        case 38400:  baud = B38400;  break;
        case 57600:  baud = B57600;  break;
        case 115200: baud = B115200; break;
        case 230400: baud = B230400; break;
        case 460800: baud = B460800; break;
        case 921600: baud = B921600; break;
        default:
            std::cerr << "Unsupported baud rate: " << baud_rate << std::endl;
            return false;
    }
    
    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);
    
    // Configure serial parameters
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_iflag &= ~IGNBRK;                         // disable break processing
    tty.c_lflag = 0;                                // no signaling chars, no echo, no canonical processing
    tty.c_oflag = 0;                                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;                            // read doesn't block
    tty.c_cc[VTIME] = 5;                            // 0.5 seconds read timeout
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);         // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);                // ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD);              // shut off parity
    tty.c_cflag &= ~CSTOPB;                         // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;                        // no hardware flowcontrol
    
    if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
        std::cerr << "Error setting serial attributes: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

bool MavlinkHandler::sendSerial(const uint8_t* buffer, uint16_t length)
{
    if (serial_fd_ < 0) {
        std::cerr << "Serial port not open" << std::endl;
        return false;
    }
    
    // Debug: Print what we're about to send
    std::cout << "Sending " << length << " bytes via serial: ";
    for (int i = 0; i < length && i < 20; i++) {  // Show first 20 bytes max
        printf("%02X ", buffer[i]);
    }
    if (length > 20) std::cout << "...";
    std::cout << std::endl;
    
    ssize_t bytes_written = write(serial_fd_, buffer, length);
    if (bytes_written < 0) {
        std::cerr << "Error writing to serial port: " << strerror(errno) 
                  << " (errno: " << errno << ")" << std::endl;
        return false;
    }
    
    if (bytes_written != length) {
        std::cerr << "Warning: Only wrote " << bytes_written 
                  << " of " << length << " bytes to serial port" << std::endl;
        return false;
    }
    
    // Ensure data is transmitted
    if (tcdrain(serial_fd_) != 0) {
        std::cerr << "Error draining serial port: " << strerror(errno) << std::endl;
        return false;
    }
    
    std::cout << "Successfully sent " << bytes_written << " bytes via serial" << std::endl;
    return true;
}

void MavlinkHandler::closeSerial()
{
    if (serial_fd_ >= 0) {
        close(serial_fd_);
        serial_fd_ = -1;
        std::cout << "Serial port closed" << std::endl;
    }
}
