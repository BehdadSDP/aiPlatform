# Flight Mode Monitoring in Tracking Visualization

## Overview
The tracking visualization window now displays real-time flight mode information received from the flight controller via MAVLink heartbeat messages.

## Display Information

### Flight Mode Display
Located in the tracking visualization window at position (10, 120):
- **Format**: "Flight Mode: [MODE_NAME]"
- **Color**: 
  - **Green** when in ALT_HOLD mode (mode 2) - indicating active tracking mode
  - **Gray** for all other modes

### MAVLink Connection Status
Located at position (10, 150):
- **"MAVLink: CONNECTED"** (Green) - Communication active
- **"MAVLink: DISCONNECTED"** (Red) - No communication

## Supported Flight Modes

The system recognizes the following ArduPilot/ArduCopter flight modes:

| Mode # | Mode Name      | Description                                    |
|--------|----------------|------------------------------------------------|
| 0      | STABILIZE      | Manual control with attitude stabilization     |
| 1      | ACRO           | Acrobatic mode - rate control                  |
| 2      | ALT_HOLD       | Altitude hold - preferred for tracking         |
| 3      | AUTO           | Autonomous waypoint navigation                 |
| 4      | GUIDED         | Guided mode for external control               |
| 5      | LOITER         | Position hold using GPS                        |
| 6      | RTL            | Return to launch                               |
| 7      | CIRCLE         | Circle around a point                          |
| 9      | LAND           | Automated landing                              |
| 11     | DRIFT          | Drift mode for gentle flight                   |
| 13     | SPORT          | Sport mode - more responsive                   |
| 14     | FLIP           | Automated flip maneuver                        |
| 15     | AUTOTUNE       | Automatic PID tuning                           |
| 16     | POSHOLD        | Position hold with GPS                         |
| 17     | BRAKE          | Rapid braking                                  |
| 18     | THROW          | Throw mode for hand launching                  |
| 19     | AVOID_ADSB     | Avoidance mode                                 |
| 20     | GUIDED_NOGPS   | Guided mode without GPS                        |
| 21     | SMART_RTL      | Smart return to launch                         |
| 22     | FLOWHOLD       | Flow hold using optical flow                   |
| 23     | FOLLOW         | Follow mode                                    |
| 24     | ZIGZAG         | Zigzag pattern                                 |
| 25     | SYSTEMID       | System identification                          |
| 26     | AUTOROTATE     | Autorotation for helicopters                   |
| 27     | AUTO_RTL       | Automatic RTL on failsafe                      |

**Unknown modes** are displayed as: "UNKNOWN(X)" where X is the mode number.

## Visual Layout

```
┌─────────────────────────────────────────────────┐
│ Roll: 1550 Pitch: 1480                    (30px)│
│ Filtered Error X: 25 Y: -15               (60px)│
│ RC Commands: ACTIVE                       (90px)│
│ Flight Mode: ALT_HOLD (GREEN)            (120px)│
│ MAVLink: CONNECTED (GREEN)               (150px)│
│                                                  │
│            [Tracking visualization]              │
│                                                  │
└─────────────────────────────────────────────────┘
```

## Code Integration

### In application.cpp (trackingThread):
```cpp
// Get flight mode and MAVLink connection status
uint32_t flightMode = 0;
bool mavlinkConnected = false;
if (m_mavlink && m_mavlinkEnabled) {
    flightMode = m_mavlink->getCurrentFlightMode();
    mavlinkConnected = m_mavlink->isConnected();
}

m_visualizer.visualizeTracking(frame, ..., flightMode, mavlinkConnected);
```

### In visualizer.cpp:
```cpp
// Display flight mode
std::string flightModeText = "Flight Mode: " + getFlightModeName(flightMode);
cv::Scalar flightModeColor = (flightMode == 2) ? cv::Scalar(0, 255, 0) : cv::Scalar(200, 200, 200);
cv::putText(frame, flightModeText, cv::Point(10, 120), ...);
```

## How It Works

1. **MAVLink Heartbeat Reception**:
   - Flight controller sends heartbeat messages at ~1Hz
   - Messages contain `custom_mode` field with flight mode number
   - `Mavlink::handleReceivedMessage()` updates `current_flight_mode_`

2. **Mode Retrieval**:
   - Application calls `m_mavlink->getCurrentFlightMode()`
   - Returns current `custom_mode` value from last heartbeat

3. **Mode Display**:
   - `Visualizer::getFlightModeName()` converts number to string
   - Text is rendered on tracking frame with appropriate color
   - ALT_HOLD (mode 2) highlighted in green for tracking operations

## Usage for Tracking Operations

### Recommended Flight Mode: ALT_HOLD (2)
The tracking system works best in **ALT_HOLD** mode because:
- Maintains altitude automatically
- Allows pitch/roll RC override for tracking
- Stable platform for vision-based tracking
- Easy to switch to manual control if needed

### Flight Mode Check in Code:
```cpp
if (m_mavlink->current_flight_mode_ == 2) {  // ALT_HOLD mode
    // Send tracking commands
    cv::Point2f rawError = m_navigationUnit.calculateError(...);
    ControlOutputs controlOutputs = m_navigationUnit.generateControlCommands(...);
}
```

## Troubleshooting

### Flight Mode Shows "UNKNOWN(X)"
- **Cause**: Flight controller using non-standard mode numbers
- **Solution**: Check your flight controller documentation and update `getFlightModeName()` switch statement

### Flight Mode Not Updating
- **Check**: MAVLink connection status displays "CONNECTED"
- **Verify**: Heartbeat messages being received (check console output)
- **Test**: Use `scripts/test_mavlink_messages.sh` to verify communication

### Wrong Flight Mode Displayed
- **Verify**: Flight controller type (ArduPilot vs PX4 have different mode numbers)
- **Check**: Mode on flight controller matches display
- **Note**: PX4 uses different custom_mode values - update `getFlightModeName()` accordingly

## Files Modified

1. **include/visualizer.h**
   - Added `flightMode` and `mavlinkConnected` parameters to `visualizeTracking()`
   - Added `getFlightModeName()` helper method

2. **src/visualizer.cpp**
   - Implemented `getFlightModeName()` with full mode mapping
   - Updated `visualizeTracking()` to display flight mode and connection status

3. **src/application.cpp**
   - Added flight mode retrieval in tracking thread
   - Passed flight mode and connection status to visualizer

## Future Enhancements

Potential improvements:
- Display armed/disarmed status
- Show battery voltage/percentage
- GPS status and satellite count
- System status warnings
- Mode-specific guidance text
- Audio alerts for mode changes

## References

- ArduPilot Flight Modes: https://ardupilot.org/copter/docs/flight-modes.html
- MAVLink Protocol: https://mavlink.io/en/
- Custom Mode Field: https://mavlink.io/en/messages/common.html#HEARTBEAT
