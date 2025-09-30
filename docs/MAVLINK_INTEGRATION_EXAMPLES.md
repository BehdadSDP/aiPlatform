# MAVLink Integration Examples

## 🚁 **Integration Overview**

The MAVLink protocol is now integrated with the AI Platform's tracking system, allowing real-time communication with flight controllers based on detection and tracking events.

## 🎯 **Current Integration Points**

### **1. When Tracking Starts**
```cpp
// In trackingThread() when new tracking begins:
if (m_mavlink && m_mavlinkEnabled) {
    std::string className = m_modelManager.getClassNames()[newClassId];
    std::cout << "🛡️ Tracking started for: " << className << std::endl;
    
    // Set flight mode to GUIDED when target detected
    m_mavlink->setFlightMode(4);  // GUIDED mode
    
    // Custom actions based on detected object type
    if (newClassId == 0) {  // High-priority target
        std::cout << "🎯 High-priority target detected" << std::endl;
        // m_mavlink->land();  // Emergency response
    }
}
```

### **2. When Tracking is Lost**
```cpp
// In trackingThread() when tracking fails:
if (wasTracking && !m_controlUnit.isTracking() && m_mavlink && m_mavlinkEnabled) {
    std::cout << "❌ Tracking lost - Sending MAVLink notification" << std::endl;
    // Return to original flight mode
    m_mavlink->setFlightMode(0);  // STABILIZE mode
}
```

## 🔧 **Configuration**

### **Enable/Disable MAVLink**
```ini
[mavlink]
enabled=1  # 0=disabled, 1=enabled
```

### **UART Settings**
```ini
uart_device=/dev/ttyAMA0
uart_baud_rate=57600
```

### **System IDs**
```ini
system_id=2           # This companion computer
component_id=1        # Component ID
target_system_id=1    # Flight controller
```

## 🎨 **Customization Examples**

### **Face Detection Integration**
```cpp
// In tracking thread for face detection model
if (m_modelManager.getCurrentModelType() == ModelType::FACE_DETECTION) {
    if (newClassId == 0) {  // Face detected
        std::cout << "👤 Face detected - Privacy mode activated" << std::endl;
        
        // Example: Return to launch for privacy
        m_mavlink->returnToLaunch();
        
        // Or set specific flight mode
        m_mavlink->setFlightMode(6);  // RTL mode
    }
}
```

### **Helmet Detection Integration**
```cpp
// In tracking thread for helmet detection model
if (m_modelManager.getCurrentModelType() == ModelType::HELMET_DETECTION) {
    if (newClassId == 0) {  // Person without helmet
        std::cout << "⚠️ Safety violation detected - No helmet" << std::endl;
        
        // Example: Land immediately for safety
        m_mavlink->land();
        
        // Or hover in place
        m_mavlink->setFlightMode(17);  // BRAKE mode
    }
}
```

### **Vehicle Detection Integration**
```cpp
// In tracking thread for vehicle detection model
if (m_modelManager.getCurrentModelType() == ModelType::COCO_GENERAL) {
    if (newClassId == 2) {  // Car detected (COCO class 2)
        std::cout << "🚗 Vehicle detected - Following mode" << std::endl;
        
        // Example: Switch to guided mode for following
        m_mavlink->setFlightMode(4);  // GUIDED mode
    }
}
```

## 🛡️ **Advanced Integration Scenarios**

### **1. Position-Based Commands**
```cpp
// Calculate relative position from detection box
cv::Rect detectionBox = m_controlUnit.getLastTrackBox();
int frame_center_x = 320;  // Assuming 640px width
int frame_center_y = 240;  // Assuming 480px height

float offset_x = (detectionBox.x + detectionBox.width/2 - frame_center_x) / 100.0f;
float offset_y = (detectionBox.y + detectionBox.height/2 - frame_center_y) / 100.0f;

// Send position offset to keep target centered
if (abs(offset_x) > 0.5f || abs(offset_y) > 0.5f) {
    std::cout << "🎯 Adjusting position to center target" << std::endl;
    // Note: This would require position control methods in MAVLink class
    // m_mavlink->setPositionTargetLocalNED(offset_x, offset_y, 0.0f);
}
```

### **2. Tracking Quality-Based Actions**
```cpp
// Monitor tracking confidence/quality
static int tracking_lost_count = 0;
static int stable_tracking_count = 0;

if (m_controlUnit.isTracking()) {
    stable_tracking_count++;
    tracking_lost_count = 0;
    
    // After stable tracking for 5 seconds
    if (stable_tracking_count > 100) {  // Assuming 20 FPS
        std::cout << "✅ Stable tracking achieved" << std::endl;
        // Could switch to more aggressive following mode
        stable_tracking_count = 0;  // Reset counter
    }
} else {
    tracking_lost_count++;
    stable_tracking_count = 0;
    
    // If tracking lost for more than 2 seconds
    if (tracking_lost_count > 40) {  // Assuming 20 FPS
        std::cout << "⚠️ Tracking lost for extended period" << std::endl;
        m_mavlink->setFlightMode(0);  // Return to STABILIZE
        tracking_lost_count = 0;  // Reset counter
    }
}
```

### **3. Multi-Target Scenarios**
```cpp
// Handle multiple detections (requires detection manager modification)
std::vector<cv::Rect> allDetections = detectionManager.getAllDetections();

if (allDetections.size() > 1) {
    std::cout << "⚠️ Multiple targets detected (" << allDetections.size() << ")" << std::endl;
    
    // Example: Emergency protocols for multiple threats
    if (allDetections.size() > 3) {
        std::cout << "🚨 High-density area - Emergency RTL" << std::endl;
        m_mavlink->returnToLaunch();
    }
}
```

## 📊 **Flight Mode Reference**

### **ArduPilot Flight Modes**
```cpp
// Common ArduPilot flight modes
m_mavlink->setFlightMode(0);   // STABILIZE - Manual control
m_mavlink->setFlightMode(1);   // ACRO - Acrobatic mode
m_mavlink->setFlightMode(2);   // ALT_HOLD - Altitude hold
m_mavlink->setFlightMode(3);   // AUTO - Mission mode
m_mavlink->setFlightMode(4);   // GUIDED - External control
m_mavlink->setFlightMode(5);   // LOITER - Position hold
m_mavlink->setFlightMode(6);   // RTL - Return to launch
m_mavlink->setFlightMode(7);   // CIRCLE - Circle mode
m_mavlink->setFlightMode(9);   // LAND - Land mode
m_mavlink->setFlightMode(17);  // BRAKE - Stop in place
```

## 🔐 **Safety Considerations**

### **1. Always Check MAVLink Status**
```cpp
if (m_mavlink && m_mavlinkEnabled && m_mavlink->isActive()) {
    // Send commands only when connection is verified
    m_mavlink->setFlightMode(mode);
}
```

### **2. Implement Timeouts**
```cpp
static auto last_command_time = std::chrono::steady_clock::now();
auto now = std::chrono::steady_clock::now();
auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_command_time);

// Don't send commands too frequently
if (elapsed.count() >= 2) {  // Minimum 2 seconds between commands
    m_mavlink->setFlightMode(mode);
    last_command_time = now;
}
```

### **3. Graceful Degradation**
```cpp
try {
    m_mavlink->setFlightMode(4);
} catch (const std::exception& e) {
    std::cerr << "MAVLink command failed: " << e.what() << std::endl;
    // Continue normal operation without MAVLink
}
```

## 🎯 **Integration Benefits**

✅ **Real-time Response**: Immediate flight controller commands based on AI detection  
✅ **Safety Integration**: Automatic safety protocols for different scenarios  
✅ **Flexible Configuration**: Easy enable/disable and customization  
✅ **Multi-Model Support**: Different actions for different AI models  
✅ **Robust Error Handling**: Graceful degradation when MAVLink fails  

## 🚀 **Usage**

1. **Configure MAVLink** in `config/config.txt`
2. **Enable UART** on Raspberry Pi 5
3. **Connect flight controller** via UART
4. **Run the AI Platform** - MAVLink commands will be sent automatically based on tracking events

The integration provides a powerful foundation for building autonomous systems that combine computer vision with flight control capabilities.
