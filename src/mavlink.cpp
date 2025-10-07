#include "include/mavlink.h"
#include <iostream>
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <cstring> // For memset

Mavlink::Mavlink(uint8_t system_id, uint8_t component_id)
    : system_id_(system_id)
    , component_id_(component_id)
    , target_system_id_(1)  // Default target is flight controller with ID 1
    , running_(false)
    , serial_fd_(-1)
    , serial_device_("")
    , baud_rate_(57600)
    , uart_initialized_(false)
    , current_flight_mode_(0)
    , current_base_mode_(0)
{
    std::cout << "Mavlink initialized with System ID: " << static_cast<int>(system_id_)
              << ", Component ID: " << static_cast<int>(component_id_) << std::endl;
}

Mavlink::~Mavlink()
{
    stop();
}

bool Mavlink::initializeUART(const std::string& serial_device, int baud_rate)
{
    // Store UART settings
    serial_device_ = serial_device;
    baud_rate_ = baud_rate;
    
    std::cout << "Initializing UART on Raspberry Pi 5..." << std::endl;
    std::cout << "Device: " << serial_device << ", Baud Rate: " << baud_rate << std::endl;
    
    // Check if device exists first
    if (access(serial_device.c_str(), F_OK) != 0) {
        std::cerr << "ERROR: Serial device " << serial_device << " does not exist" << std::endl;
        std::cerr << "Available devices: " << std::endl;
        system("ls -la /dev/tty* | grep -E '(ttyAMA|ttyS|serial)'");
        return false;
    }
    
    // Check permissions
    if (access(serial_device.c_str(), R_OK | W_OK) != 0) {
        std::cerr << "ERROR: No permission to access " << serial_device << std::endl;
        std::cerr << "Solutions:" << std::endl;
        std::cerr << "  1. Add user to dialout group: sudo usermod -a -G dialout $USER" << std::endl;
        std::cerr << "  2. Set device permissions: sudo chmod 666 " << serial_device << std::endl;
        std::cerr << "  3. Enable UART in raspi-config: sudo raspi-config -> Interface Options -> Serial" << std::endl;
        return false;
    }
    
    // Open UART port with proper flags for Raspberry Pi 5
    serial_fd_ = open(serial_device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd_ < 0) {
        std::cerr << "ERROR: Failed to open UART device " << serial_device 
                  << ": " << strerror(errno) << " (errno: " << errno << ")" << std::endl;
        
        // Provide detailed troubleshooting for Raspberry Pi 5
        std::cerr << "Raspberry Pi 5 UART Troubleshooting:" << std::endl;
        std::cerr << "  1. Check UART is enabled: grep enable_uart /boot/config.txt" << std::endl;
        std::cerr << "  2. Check UART overlay: grep uart /boot/config.txt" << std::endl;
        std::cerr << "  3. Check cmdline: grep console /boot/cmdline.txt" << std::endl;
        std::cerr << "  4. Required in /boot/config.txt:" << std::endl;
        std::cerr << "     enable_uart=1" << std::endl;
        std::cerr << "     dtoverlay=uart0" << std::endl;
        std::cerr << "  5. Reboot after config changes: sudo reboot" << std::endl;
        return false;
    }
    
    // Configure UART parameters
    if (!configureUART(baud_rate)) {
        close(serial_fd_);
        serial_fd_ = -1;
        return false;
    }
    
    // Set non-blocking mode
    int flags = fcntl(serial_fd_, F_GETFL, 0);
    fcntl(serial_fd_, F_SETFL, flags | O_NONBLOCK);
    
    uart_initialized_ = true;
    
    std::cout << "✅ UART initialized successfully on " << serial_device 
              << " at " << baud_rate << " baud" << std::endl;
    
    return true;
}

bool Mavlink::start()
{
    if (running_) {
        std::cout << "Mavlink communication is already running" << std::endl;
        return false;
    }
    
    if (!uart_initialized_) {
        std::cerr << "ERROR: UART not initialized. Call initializeUART() first." << std::endl;
        return false;
    }
    
    running_ = true;
    
    std::cout << "✅ Mavlink communication started" << std::endl;
    return true;
}

void Mavlink::stop()
{
    if (!running_) {
        return;
    }
    
    std::cout << "Stopping Mavlink communication..." << std::endl;
    
    
    // Stop all threads
    running_ = false;
    
    std::cout << "✅ Mavlink communication stopped" << std::endl;
}


// === FLIGHT CONTROLLER COMMANDS ===

bool Mavlink::armDisarm(bool arm, bool force)
{
    // Use correct force parameter for different autopilots
    float forceParam = 0.0f;
    if (force) {
        // Try different force parameters for different autopilots
        forceParam = 21196.0f;  // PX4 force parameter
        // Alternative: forceParam = 2989.0f;  // ArduPilot force parameter
    }
    
    bool result = sendCommandLong(
        MAV_CMD_COMPONENT_ARM_DISARM,
        arm ? 1.0f : 0.0f,  // param1: 1 to arm, 0 to disarm
        forceParam,         // param2: force arm/disarm
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    );
    
    if (result) {
        // Add a small delay to allow for command processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } else {
        std::cerr << "❌ Failed to send ARM/DISARM command" << std::endl;
    }
    
    return result;
}


bool Mavlink::takeoff(float altitude, double latitude, double longitude)
{
    std::cout << "Requesting takeoff to altitude: " << altitude << "m" << std::endl;
    
    return sendCommandLong(
        MAV_CMD_NAV_TAKEOFF,
        0.0f,  // param1: pitch angle (optional)
        0.0f,  // param2: empty
        0.0f,  // param3: empty
        0.0f,  // param4: yaw angle (optional)
        static_cast<float>(latitude),   // param5: latitude
        static_cast<float>(longitude),  // param6: longitude
        altitude  // param7: altitude
    );
}

bool Mavlink::land(double latitude, double longitude)
{
    std::cout << "Requesting landing" << std::endl;
    
    return sendCommandLong(
        MAV_CMD_NAV_LAND,
        0.0f,  // param1: abort altitude (0 for default)
        0.0f,  // param2: precision land mode
        0.0f,  // param3: empty
        0.0f,  // param4: yaw angle (optional)
        static_cast<float>(latitude),   // param5: latitude
        static_cast<float>(longitude),  // param6: longitude
        0.0f   // param7: altitude (ignored for land)
    );
}

bool Mavlink::sendManualControl(int16_t x, int16_t y, int16_t z, int16_t r, uint16_t buttons)
{
    if (!uart_initialized_) {
        std::cerr << "ERROR: Cannot send manual control - UART not initialized" << std::endl;
        return false;
    }
    
    if (!running_) {
        std::cerr << "ERROR: Cannot send manual control - MAVLink not running" << std::endl;
        return false;
    }
    
    // Clamp values to valid range (-1000 to 1000)
    x = std::max(static_cast<int16_t>(-1000), std::min(static_cast<int16_t>(1000), x));
    y = std::max(static_cast<int16_t>(-1000), std::min(static_cast<int16_t>(1000), y));
    z = std::max(static_cast<int16_t>(-1000), std::min(static_cast<int16_t>(1000), z));
    r = std::max(static_cast<int16_t>(-1000), std::min(static_cast<int16_t>(1000), r));
    
    mavlink_message_t msg;
    memset(&msg, 0, sizeof(msg)); // Initialize to prevent garbage data
    
    mavlink_msg_manual_control_pack(
        system_id_,
        component_id_,
        &msg,
        target_system_id_,  // target
        x,                  // x: forward/backward (-1000 to 1000)
        y,                  // y: left/right (-1000 to 1000)
        z,                  // z: up/down (-1000 to 1000)
        r,                  // r: yaw rotation (-1000 to 1000)
        buttons,            // buttons: button states (bitmask)
        0,                  // buttons2: additional buttons
        0,                  // enabled_extensions
        0,                  // s: additional control
        0,                  // t: additional control
        0, 0, 0, 0, 0, 0    // aux1-aux6: auxiliary controls
    );
    
    bool result = sendMessageImmediate(msg);
    
    if (result) {
        std::cout << "✅ Sent manual control - X:" << x 
                  << " Y:" << y << " Z:" << z << " R:" << r 
                  << " Buttons:0x" << std::hex << buttons << std::dec << std::endl;
    } else {
        std::cerr << "❌ Failed to send manual control command" << std::endl;
    }
    
    return result;
}

bool Mavlink::sendRCOverride(const uint16_t channels[18])
{
    if (!uart_initialized_) {
        std::cerr << "ERROR: Cannot send RC override - UART not initialized" << std::endl;
        return false;
    }
    
    if (!running_) {
        std::cerr << "ERROR: Cannot send RC override - MAVLink not running" << std::endl;
        return false;
    }
    
    mavlink_message_t msg;
    memset(&msg, 0, sizeof(msg)); // Initialize to prevent garbage data
    
    mavlink_msg_rc_channels_override_pack(
        system_id_,
        component_id_,
        &msg,
        target_system_id_,  // target system
        MAV_COMP_ID_AUTOPILOT1,  // target component
        channels[0],   // chan1_raw: Roll (Aileron)
        channels[1],   // chan2_raw: Pitch (Elevator) 
        channels[2],   // chan3_raw: Throttle
        channels[3],   // chan4_raw: Yaw (Rudder)
        channels[4],   // chan5_raw: Aux1
        channels[5],   // chan6_raw: Aux2
        channels[6],   // chan7_raw: Aux3
        channels[7],   // chan8_raw: Aux4
        channels[8],   // chan9_raw: Aux5
        channels[9],   // chan10_raw: Aux6
        channels[10],  // chan11_raw: Aux7
        channels[11],  // chan12_raw: Aux8
        channels[12],  // chan13_raw: Aux9
        channels[13],  // chan14_raw: Aux10
        channels[14],  // chan15_raw: Aux11
        channels[15],  // chan16_raw: Aux12
        channels[16],  // chan17_raw: Aux13
        channels[17]   // chan18_raw: Aux14
    );
    
    bool result = sendMessageImmediate(msg);
    
    if (!result) {
        std::cerr << "❌ Failed to send RC override command" << std::endl;
    }
    
    return result;
}


// === PRIVATE METHODS ===


bool Mavlink::sendMessageImmediate(const mavlink_message_t& msg)
{
    if (!uart_initialized_) {
        std::cerr << "ERROR: UART not initialized" << std::endl;
        return false;
    }
    
    // Create a copy of the message to avoid const issues
    mavlink_message_t msg_copy = msg;
    
    // Convert MAVLink message to buffer
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    uint16_t length = mavlink_msg_to_send_buffer(buffer, &msg_copy);
    
    // Check if message starts with correct magic byte
    if (length > 0) {
        if (buffer[0] != MAVLINK_STX && buffer[0] != MAVLINK_STX_MAVLINK1) {
            std::cerr << "❌ Message has invalid magic byte: 0x" << std::hex << (int)buffer[0] << std::dec << std::endl;
            std::cerr << "Expected: 0xFD (MAVLink 2.0) or 0xFE (MAVLink 1.0)" << std::endl;
            return false;
        }
    }
    
    // Send via UART
    bool result = sendUART(buffer, length);
    
    return result;
}


bool Mavlink::configureUART(int baud_rate)
{
    struct termios tty;
    
    if (tcgetattr(serial_fd_, &tty) != 0) {
        std::cerr << "ERROR: Failed to get UART attributes: " << strerror(errno) << std::endl;
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
            std::cerr << "ERROR: Unsupported baud rate: " << baud_rate << std::endl;
            return false;
    }
    
    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);
    
    // Configure serial parameters for MAVLink
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_iflag &= ~IGNBRK;                         // disable break processing
    tty.c_lflag = 0;                                // no signaling chars, no echo, no canonical processing
    tty.c_oflag = 0;                                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;                            // read doesn't block
    tty.c_cc[VTIME] = 1;                            // 0.1 seconds read timeout
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);         // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);                // ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD);              // shut off parity
    tty.c_cflag &= ~CSTOPB;                         // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;                        // no hardware flowcontrol
    
    // Additional settings for reliable MAVLink communication
    tty.c_iflag &= ~(INLCR | ICRNL | IUCLC | IMAXBEL);
    tty.c_oflag &= ~(OPOST | ONLCR | OCRNL | ONOCR | ONLRET | OFILL | OFDEL);
    tty.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL | IEXTEN);
    
    if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
        std::cerr << "ERROR: Failed to set UART attributes: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Flush buffers
    tcflush(serial_fd_, TCIOFLUSH);
    
    std::cout << "UART configured successfully at " << baud_rate << " baud" << std::endl;
    return true;
}

bool Mavlink::sendUART(const uint8_t* buffer, uint16_t length)
{
    std::lock_guard<std::mutex> lock(uart_mutex_);
    
    if (serial_fd_ < 0) {
        std::cerr << "ERROR: UART not open" << std::endl;
        return false;
    }
    
    ssize_t bytes_written = write(serial_fd_, buffer, length);
    if (bytes_written < 0) {
        std::cerr << "ERROR: Failed to write to UART: " << strerror(errno) 
                  << " (errno: " << errno << ")" << std::endl;
        return false;
    }
    
    if (bytes_written != length) {
        std::cerr << "WARNING: Only wrote " << bytes_written 
                  << " of " << length << " bytes to UART" << std::endl;
        return false;
    }
    
    // Flush the output buffer to ensure data is sent immediately
    if (tcdrain(serial_fd_) != 0) {
        std::cerr << "WARNING: Failed to drain UART buffer" << std::endl;
    }
    
    return true;
}

bool Mavlink::sendCommandLong(uint16_t command, float param1, float param2, 
                             float param3, float param4, float param5, 
                             float param6, float param7)
{
    if (!uart_initialized_) {
        std::cerr << "ERROR: Cannot send command - UART not initialized" << std::endl;
        return false;
    }
    
    if (!running_) {
        std::cerr << "ERROR: Cannot send command - MAVLink not running" << std::endl;
        return false;
    }
    
    mavlink_message_t msg;
    memset(&msg, 0, sizeof(msg)); // Initialize to prevent garbage data
    
    mavlink_msg_command_long_pack(
        system_id_,
        component_id_,
        &msg,
        target_system_id_,
        MAV_COMP_ID_AUTOPILOT1,
        command,
        0,  // confirmation
        param1, param2, param3, param4, param5, param6, param7
    );
    
    bool result = sendMessageImmediate(msg);
    
    if (result) {
        std::cout << "✅ Sent MAVLink command: " << command 
                  << " (ID: " << msg.msgid << ")" << std::endl;
        
        // Log command parameters for debugging
        if (command == MAV_CMD_COMPONENT_ARM_DISARM) {
            std::cout << "   ARM/DISARM params: arm=" << param1 
                      << ", force=" << param2 << std::endl;
        }
    } else {
        std::cerr << "❌ Failed to send MAVLink command: " << command << std::endl;
    }
    
    return result;
}

void Mavlink::testMAVLinkMessageFormat()
{
    // Test ARM command
    mavlink_message_t arm_msg;
    memset(&arm_msg, 0, sizeof(arm_msg)); // Initialize to prevent garbage data
    mavlink_msg_command_long_pack(
        system_id_,
        component_id_,
        &arm_msg,
        target_system_id_,
        MAV_COMP_ID_AUTOPILOT1,
        MAV_CMD_COMPONENT_ARM_DISARM,
        0,  // confirmation
        1.0f, // param1: arm
        21196.0f, // param2: force
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f // param3-7
    );
    
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    uint16_t arm_length = mavlink_msg_to_send_buffer(buffer, &arm_msg);
    
    // Verify message format
    if (buffer[0] != MAVLINK_STX && buffer[0] != MAVLINK_STX_MAVLINK1) {
        std::cerr << "❌ Invalid magic byte detected in test message" << std::endl;
    }
}

bool Mavlink::processIncomingMessages()
{
    if (!uart_initialized_ || !running_) {
        return false;
    }
    
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    mavlink_message_t msg;
    mavlink_status_t status;
    
    // Read available data from UART
    ssize_t bytes_read = read(serial_fd_, buffer, sizeof(buffer));
    
    if (bytes_read <= 0) {
        // No data available or error
        if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "Error reading from UART: " << strerror(errno) << std::endl;
            return false;
        }
        return true; // No data available, but not an error
    }
    
    // Process each byte to parse MAVLink messages
    for (ssize_t i = 0; i < bytes_read; i++) {
        uint8_t result = mavlink_parse_char(MAVLINK_COMM_0, buffer[i], &msg, &status);
        
        if (result == MAVLINK_FRAMING_OK) {
            // Successfully parsed a complete message
            handleReceivedMessage(msg);
        } else if (result == MAVLINK_FRAMING_BAD_CRC) {
            std::cerr << "Bad CRC in MAVLink message" << std::endl;
        }
    }
    
    return true;
}

void Mavlink::handleReceivedMessage(const mavlink_message_t& msg)
{
    switch (msg.msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT: {
            mavlink_heartbeat_t heartbeat;
            mavlink_msg_heartbeat_decode(&msg, &heartbeat);
            
            // Update current flight mode and base mode
            current_flight_mode_ = heartbeat.custom_mode;
            current_base_mode_ = heartbeat.base_mode;
            
            std::cout << "Base Mode: 0x" << std::hex << static_cast<int>(heartbeat.base_mode) 
                      << ", Custom Mode: " << std::dec << heartbeat.custom_mode << std::endl;
            break;
        }
        
        case MAVLINK_MSG_ID_SYS_STATUS: {
            mavlink_sys_status_t sys_status;
            mavlink_msg_sys_status_decode(&msg, &sys_status);
            
            std::cout << "📊 SYS_STATUS received - Battery: " << sys_status.voltage_battery 
                      << "mV, Current: " << sys_status.current_battery << "cA" << std::endl;
            break;
        }
        
        case MAVLINK_MSG_ID_ATTITUDE: {
            mavlink_attitude_t attitude;
            mavlink_msg_attitude_decode(&msg, &attitude);
            
            std::cout << "🛩️  ATTITUDE received - Roll: " << attitude.roll 
                      << ", Pitch: " << attitude.pitch << ", Yaw: " << attitude.yaw << std::endl;
            break;
        }
        
        default:
            std::cout << "📨 Message ID " << static_cast<int>(msg.msgid) 
                      << " received from System " << static_cast<int>(msg.sysid) 
                      << ", Component " << static_cast<int>(msg.compid) << std::endl;
            break;
    }
}

bool Mavlink::isVehicleArmed() const {
    // Check if the MAV_MODE_FLAG_SAFETY_ARMED bit is set in base_mode
    // Base mode bit flags are defined in MAVLink common protocol
    const uint8_t MAV_MODE_FLAG_SAFETY_ARMED = 128; // 0x80
    return (current_base_mode_ & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
}
