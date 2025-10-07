# Navigation Control System - Block Diagram

## Overview
This document describes the block diagram for the conditional RC override navigation control system that only sends roll/pitch commands when the tracked object is not centered in the camera frame.

## Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                              CAMERA INPUT & TRACKING                                   │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  Camera Frame    │  Object Detection  │  Tracker Manager  │  Tracked Object         │
│  (W x H)         │  (YOLO Model)      │  (VitTracker/     │  Bounding Box           │
│                  │                    │   SiamFCPP)       │  (x, y, w, h)           │
└─────────────────────────────────────────────────────────────────────────────────────┘
                                           │
                                           ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                              NAVIGATION UNIT                                           │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                         │
│  ┌─────────────────────┐      ┌─────────────────────┐                                 │
│  │   Error Calculation │      │  Center Check Logic │                                 │
│  │                     │      │                     │                                 │
│  │ Frame Center:       │      │ Frame Center in     │                                 │
│  │ (W/2, H/2)         │      │ Bounding Box?       │                                 │
│  │                     │      │                     │                                 │
│  │ Object Center:      │      │ frameCenter.x >=    │                                 │
│  │ (x+w/2, y+h/2)     │      │ box.x &&            │                                 │
│  │                     │      │ frameCenter.x <=    │                                 │
│  │ Raw Error:          │      │ box.x + box.width   │                                 │
│  │ dx = objX - frameX  │      │ &&                  │                                 │
│  │ dy = objY - frameY  │      │ frameCenter.y >=    │                                 │
│  └─────────────────────┘      │ box.y &&            │                                 │
│            │                   │ frameCenter.y <=    │                                 │
│            ▼                   │ box.y + box.height  │                                 │
│  ┌─────────────────────┐      └─────────────────────┘                                 │
│  │   Low-Pass Filter   │               │                                              │
│  │  (Exponential MA)   │               ▼                                              │
│  │                     │      ┌─────────────────────┐                                 │
│  │ Filtered Error =    │      │   Decision Logic    │                                 │
│  │ α × Raw + (1-α) ×   │      │                     │                                 │
│  │ Previous Filtered   │      │ centerInBox =       │                                 │
│  │                     │      │ isFrameCenterIn     │                                 │
│  │ α = 0.3 (default)   │      │ BoundingBox()       │                                 │
│  └─────────────────────┘      │                     │                                 │
│            │                   │ sendCommands =      │                                 │
│            ▼                   │ !centerInBox        │                                 │
│  ┌─────────────────────┐      └─────────────────────┘                                 │
│  │    PID Controller   │               │                                              │
│  │                     │               ▼                                              │
│  │ Roll Axis:          │      ┌─────────────────────┐                                 │
│  │ Kp=0.5, Ki=0.1,     │      │  RC Command Logic   │                                 │
│  │ Kd=0.05             │      │                     │                                 │
│  │                     │      │ IF sendCommands:    │                                 │
│  │ Pitch Axis:         │      │   Send RC Override  │                                 │
│  │ Kp=0.5, Ki=0.1,     │      │   channels[0] = roll│                                 │
│  │ Kd=0.05             │      │   channels[1] = pitch│                                │
│  │                     │      │                     │                                 │
│  │ Output Range:       │      │ ELSE:               │                                 │
│  │ 1300-1700 PWM       │      │   Suppress commands │                                 │
│  │ (Neutral = 1500)    │      │   (Object centered) │                                 │
│  └─────────────────────┘      └─────────────────────┘                                 │
│            │                               │                                          │
│            ▼                               ▼                                          │
│  ┌─────────────────────┐      ┌─────────────────────┐                                 │
│  │   Control Outputs   │      │   Command Status    │                                 │
│  │                     │      │                     │                                 │
│  │ roll_output: int    │      │ rc_commands_sent:   │                                 │
│  │ pitch_output: int   │      │ boolean             │                                 │
│  │ error: Point2f      │      │                     │                                 │
│  └─────────────────────┘      └─────────────────────┘                                 │
└─────────────────────────────────────────────────────────────────────────────────────┘
                                           │
                                           ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                              MAVLINK COMMUNICATION                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                         │
│  ┌─────────────────────┐                    ┌─────────────────────┐                   │
│  │   RC Override       │                    │   Flight Controller │                   │
│  │   Message           │    UART/Serial     │   (ArduPilot/PX4)   │                   │
│  │                     │ ────────────────►  │                     │                   │
│  │ MSG ID: 70          │                    │ Roll/Pitch Control  │                   │
│  │ channels[0] = roll  │                    │ Attitude Adjustment │                   │
│  │ channels[1] = pitch │                    │                     │                   │
│  │ channels[2-17] =    │                    │                     │                   │
│  │ UINT16_MAX (ignore) │                    │                     │                   │
│  └─────────────────────┘                    └─────────────────────┘                   │
└─────────────────────────────────────────────────────────────────────────────────────┘
                                           │
                                           ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                              VISUALIZATION FEEDBACK                                    │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                         │
│  ┌─────────────────────┐      ┌─────────────────────┐                                 │
│  │   Video Display     │      │   Status Overlay    │                                 │
│  │                     │      │                     │                                 │
│  │ • Tracking Box      │      │ • Roll/Pitch Values │                                 │
│  │ • Frame Center      │      │ • Filtered Error    │                                 │
│  │ • Object Center     │      │ • RC Commands:      │                                 │
│  │ • Error Line        │      │   - ACTIVE (Yellow) │                                 │
│  │                     │      │   - CENTERED        │                                 │
│  │                     │      │     (Green)         │                                 │
│  └─────────────────────┘      └─────────────────────┘                                 │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

## Control Flow Logic

### 1. **Tracking Phase**
```
Camera Frame → Object Detection → Bounding Box → Error Calculation
```

### 2. **Decision Phase**
```
Frame Center Position → Bounding Box Check → Send Commands Decision
```

### 3. **Control Phase**
```
Raw Error → Low-Pass Filter → PID Controller → RC Commands (Conditional)
```

### 4. **Output Phase**
```
Control Outputs → MAVLink (if needed) → Drone Control → Visual Feedback
```

## Key Decision Points

### Center Check Algorithm
```cpp
bool isFrameCenterInBoundingBox(objectBox, frameWidth, frameHeight) {
    frameCenter = (frameWidth/2, frameHeight/2)
    
    insideX = (frameCenter.x >= objectBox.x) && 
              (frameCenter.x <= objectBox.x + objectBox.width)
    
    insideY = (frameCenter.y >= objectBox.y) && 
              (frameCenter.y <= objectBox.y + objectBox.height)
    
    return insideX && insideY
}
```

### Command Sending Logic
```cpp
if (!centerInBox && mavlink_available) {
    // Send RC Override Commands
    channels[0] = roll_output    // Roll control
    channels[1] = pitch_output   // Pitch control
    mavlink->sendRCOverride(channels)
    rc_commands_sent = true
} else {
    // Suppress commands (object is centered)
    rc_commands_sent = false
}
```

## System States

| State | Condition | RC Commands | Visual Indicator |
|-------|-----------|-------------|------------------|
| **ACTIVE** | Frame center outside bounding box | ✅ Sent | Yellow "RC Commands: ACTIVE" |
| **CENTERED** | Frame center inside bounding box | ❌ Suppressed | Green "RC Commands: CENTERED (SUPPRESSED)" |
| **TRACKING LOST** | No valid bounding box | ❌ Not applicable | Red "Tracking Lost" |

## Benefits

1. **Efficiency**: Only sends RC commands when needed
2. **Stability**: Reduces unnecessary drone movements
3. **Smooth Control**: PID continues calculating for smooth transitions
4. **Visual Feedback**: Clear indication of system state
5. **Precision**: Uses filtered error for stable control
6. **Safety**: Maintains control bounds (1300-1700 PWM)

## Configuration Parameters

- **PID Gains**: Kp=0.5, Ki=0.1, Kd=0.05 (adjustable)
- **Filter Alpha**: 0.3 (adjustable smoothing factor)
- **PWM Range**: 1300-1700 (±200 from neutral 1500)
- **Update Rate**: Based on tracking thread frequency
