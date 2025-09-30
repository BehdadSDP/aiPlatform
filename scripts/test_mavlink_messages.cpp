#include <iostream>
#include <cstdio>
#include "include/mavlink.h"

int main() {
    std::cout << "🔍 MAVLink Message Format Test" << std::endl;
    std::cout << "==============================" << std::endl;
    
    // Test heartbeat message
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(
        1,  // system_id
        255, // component_id
        &msg,
        MAV_TYPE_GCS,
        MAV_AUTOPILOT_GENERIC,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        0,  // custom_mode
        MAV_STATE_ACTIVE
    );
    
    // Convert to buffer
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    uint16_t length = mavlink_msg_to_send_buffer(buffer, &msg);
    
    std::cout << "Heartbeat message:" << std::endl;
    std::cout << "  Length: " << length << " bytes" << std::endl;
    std::cout << "  First 16 bytes: ";
    for (int i = 0; i < std::min(16, (int)length); i++) {
        printf("0x%02X ", buffer[i]);
    }
    std::cout << std::endl;
    
    // Check magic byte
    if (buffer[0] == MAVLINK_STX) {
        std::cout << "  ✅ Correct MAVLink 2.0 magic byte (0xFD)" << std::endl;
    } else if (buffer[0] == MAVLINK_STX_MAVLINK1) {
        std::cout << "  ⚠️  MAVLink 1.0 magic byte (0xFE)" << std::endl;
    } else {
        std::cout << "  ❌ Invalid magic byte: 0x" << std::hex << (int)buffer[0] << std::dec << std::endl;
    }
    
    // Test ARM command
    mavlink_message_t arm_msg;
    mavlink_msg_command_long_pack(
        1,  // system_id
        255, // component_id
        &arm_msg,
        1,  // target_system
        MAV_COMP_ID_AUTOPILOT1,
        MAV_CMD_COMPONENT_ARM_DISARM,
        0,  // confirmation
        1.0f, // param1: arm
        21196.0f, // param2: force
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f // param3-7
    );
    
    uint16_t arm_length = mavlink_msg_to_send_buffer(buffer, &arm_msg);
    
    std::cout << "\nARM command message:" << std::endl;
    std::cout << "  Length: " << arm_length << " bytes" << std::endl;
    std::cout << "  First 16 bytes: ";
    for (int i = 0; i < std::min(16, (int)arm_length); i++) {
        printf("0x%02X ", buffer[i]);
    }
    std::cout << std::endl;
    
    // Check magic byte
    if (buffer[0] == MAVLINK_STX) {
        std::cout << "  ✅ Correct MAVLink 2.0 magic byte (0xFD)" << std::endl;
    } else if (buffer[0] == MAVLINK_STX_MAVLINK1) {
        std::cout << "  ⚠️  MAVLink 1.0 magic byte (0xFE)" << std::endl;
    } else {
        std::cout << "  ❌ Invalid magic byte: 0x" << std::hex << (int)buffer[0] << std::dec << std::endl;
    }
    
    std::cout << "\n📋 Expected MAVLink Message Format:" << std::endl;
    std::cout << "===================================" << std::endl;
    std::cout << "MAVLink 2.0 messages should start with: 0xFD" << std::endl;
    std::cout << "MAVLink 1.0 messages should start with: 0xFE" << std::endl;
    std::cout << "\nIf you see 0x00 or 0x01, the message framing is broken!" << std::endl;
    
    return 0;
}
