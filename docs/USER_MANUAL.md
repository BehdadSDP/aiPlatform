# AI Platform User Manual

## Drone Object Tracking & Navigation System

This platform enables autonomous object tracking on a drone using computer vision and MAVLink communication with ArduPilot flight controllers.

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Hardware Requirements](#hardware-requirements)
3. [Quick Start Guide](#quick-start-guide)
4. [User Interface Guide](#user-interface-guide)
5. [Configuration Settings](#configuration-settings)
6. [Flight Operations](#flight-operations)
7. [Tracking Modes](#tracking-modes)
8. [PID Tuning](#pid-tuning)
9. [Troubleshooting](#troubleshooting)

---

## System Overview

### What It Does

The AI Platform provides:
- **Object Detection**: Detects objects using YOLO, AprilTag, or color detection
- **Object Tracking**: Tracks selected objects using VitTracker, SiamFC++, or CSRT
- **Autonomous Navigation**: Controls drone pitch/yaw to keep tracked object centered
- **MAVLink Integration**: Communicates with ArduPilot via UART

### Architecture

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Camera    │───►│  Detection  │───►│  Tracking   │───►│  Navigation │
│   (Pi Cam)  │    │  (YOLO/Tag) │    │ (VitTracker)│    │  (PID+MAV)  │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
                                                                │
                                                                ▼
                                                         ┌─────────────┐
                                                         │  ArduPilot  │
                                                         │   (Pixhawk) │
                                                         └─────────────┘
```

---

## Hardware Requirements

| Component | Requirement |
|-----------|-------------|
| **Companion Computer** | Raspberry Pi 5 |
| **Camera** | Pi Camera Module (libcamera compatible) |
| **Flight Controller** | Pixhawk/ArduPilot compatible |
| **UART Connection** | Pi GPIO → Flight Controller TELEM port |
| **Baud Rate** | 57600 (default) |

### UART Wiring

```
Raspberry Pi 5          Flight Controller
─────────────────       ─────────────────
GPIO 14 (TX) ──────────► RX (TELEM2)
GPIO 15 (RX) ◄────────── TX (TELEM2)
GND ───────────────────── GND
```

---

## Quick Start Guide

### Step 1: Power On
1. Power the Raspberry Pi 5
2. Connect to the Pi via VNC or direct display
3. Navigate to the project folder

### Step 2: Launch Application
```bash
cd /home/pi5/shared_folder/aiPlatform
./ai
```

### Step 3: Configure Detection
1. Select detection model from dropdown:
   - **Vehicle** (0): Detect cars, trucks
   - **Helmet** (1): Detect helmets
   - **Face** (2): Detect faces
   - **Color** (3): Detect colored objects
   - **AprilTag** (4): Detect AprilTag markers

### Step 4: Start Detection
1. Click **Start** button
2. Detection window shows detected objects

### Step 5: Select Target for Tracking
- **Automatic**: Set `selection_strategy` in config (0=highest confidence)
- **Manual**: Set `selection_strategy=6`, then click on desired object

### Step 6: Enable MAVLink (for drone control)
1. Check **Enable MAVLink** checkbox
2. Wait for "MAVLink: CONNECTED" status
3. Click **GUIDED_NOGPS** button to enter control mode
4. Click **Arm** button to arm the drone

### Step 7: Tracking Starts Automatically
- The drone will pitch/yaw to keep the object centered
- FPS display shows tracking performance

---

## User Interface Guide

### Main Window Layout

```
┌────────────────────────────────────────────────────────────────────┐
│                         AI Platform                                 │
├────────────────────────────────────┬───────────────────────────────┤
│                                    │  Model Settings               │
│                                    │  ┌─────────────────────────┐  │
│      Detection / Tracking          │  │ Detection Model: [▼]    │  │
│           Video Feed               │  │ ○ Detection+Tracking    │  │
│                                    │  │ ○ Detection Only        │  │
│                                    │  └─────────────────────────┘  │
│                                    │                               │
│                                    │  Tracker Settings             │
│                                    │  ┌─────────────────────────┐  │
│                                    │  │ Tracker: [VitTracker ▼] │  │
│                                    │  │ ☐ Show Tracking Path    │  │
│                                    │  └─────────────────────────┘  │
│                                    │                               │
│                                    │  MAVLink Control              │
│                                    │  ┌─────────────────────────┐  │
│                                    │  │ ☑ Enable MAVLink        │  │
│                                    │  │ Status: Connected       │  │
│                                    │  │ Mode: GUIDED_NOGPS      │  │
│                                    │  │ Altitude: 10.5 m        │  │
│                                    │  │ [GUIDED_NOGPS][ALT_HOLD]│  │
│                                    │  │ [Arm][Takeoff][Land]    │  │
│                                    │  └─────────────────────────┘  │
│                                    │                               │
│                                    │  [Start] [Stop]               │
│                                    │  FPS: 25.0                    │
├────────────────────────────────────┴───────────────────────────────┤
│  Log Messages                                                       │
└────────────────────────────────────────────────────────────────────┘
```

### Tracking Window Overlay

When tracking is active, the video shows:

| Element | Description | Color |
|---------|-------------|-------|
| **Bounding Box** | Tracked object boundary | Green |
| **Center Cross** | Object center point | Yellow |
| **Dead Zone Lines** | Pitch/yaw tolerance boundaries | Red |
| **Connection Line** | Frame center to object center | Yellow |
| **Status Bar** | MAVLink, Mode, RC, Pitch, Yaw, Error | Red text |
| **Tracking Label** | "Tracking: [class]" | Green |

---

## Configuration Settings

Edit `config/config.txt` to customize behavior:

### Camera Settings
```ini
[camera]
resolution_index=4      # 4=custom resolution
width=1920              # Frame width
height=1080             # Frame height
frame_rate=20           # FPS
rotation_angle=0        # 0, 90, 180, or 270
```

### Detection Settings
```ini
[detection]
mode=1                  # 0=interval, 1=continuous
selection_strategy=0    # 0=highest confidence, 6=manual
```

### Tracker Settings
```ini
[tracking]
tracker_type=0          # 0=VitTracker, 1=SiamFC++, 2=CSRT
```

### MAVLink Settings
```ini
[mavlink]
enabled=1               # 0=disabled, 1=enabled
uart_device=/dev/ttyAMA0
uart_baud_rate=57600
```

### Navigation (PID) Settings
```ini
[navigation]
centering_radius=200    # Dead zone size (pixels)
yaw_dead_zone_width=100 # Horizontal dead zone
filter_alpha=0.1        # Low-pass filter (0.0-1.0)
```

---

## Flight Operations

### Flight Modes

| Mode | Code | Use For |
|------|------|---------|
| **GUIDED_NOGPS** | 20 | Autonomous tracking (SET_ATTITUDE_TARGET works) |
| **ALT_HOLD** | 2 | Manual control (no attitude commands) |
| **STABILIZE** | 0 | Manual control |

### Control Buttons

| Button | Action |
|--------|--------|
| **GUIDED_NOGPS** | Switch to GUIDED_NOGPS mode (for tracking) |
| **ALT_HOLD** | Switch to ALT_HOLD mode (manual) |
| **Arm** | Arm the motors |
| **Takeoff** | Automatic takeoff (10m default) |
| **Land** | Automatic landing |

### Safety Notes

⚠️ **WARNING**: 
- Always have manual RC control available
- Test in simulation (SITL) before real flight
- Keep propellers removed during ground testing
- The system will auto-arm in GUIDED_NOGPS mode when tracking starts

---

## Tracking Modes

### Detection Models

| Model | Config Value | Description |
|-------|--------------|-------------|
| Vehicle | `model_type=0` | Cars, trucks, motorcycles |
| Helmet | `model_type=1` | Safety helmets |
| Face | `model_type=2` | Human faces |
| Color | `model_type=3` | Colored objects (configurable) |
| AprilTag | `model_type=4` | Fiducial markers |

### Tracker Types

| Tracker | Config Value | Speed | Accuracy | Notes |
|---------|--------------|-------|----------|-------|
| VitTracker | `tracker_type=0` | Slow | High | Best for drones |
| SiamFC++ | `tracker_type=1` | Medium | High | Good balance |
| CSRT | `tracker_type=2` | Fast | Medium | No model needed |

---

## PID Tuning

### Current Default Gains (in `navigation_unit.h`)

```cpp
m_pitchKp = 0.5f;   // Proportional
m_pitchKi = 0.1f;   // Integral
m_pitchKd = 0.05f;  // Derivative
```

### Tuning Guide

| Symptom | Adjust |
|---------|--------|
| Slow response | Increase `Kp` |
| Oscillation | Decrease `Kp`, increase `Kd` |
| Steady-state error | Increase `Ki` |
| Jerky movement | Decrease `filter_alpha` (more smoothing) |
| Drifting in dead zone | Ensure `integral = 0` in dead zone |

### Control Flow

```
Error (pixels) → Low-Pass Filter → PID → Scale to Angle → Clamp → MAVLink
     ↓                                         ↓
 ±360 max                                  ±5° max
```

### Dead Zone Behavior

When object is within dead zone:
- **Pitch output** = 0
- **Integral** = 0 (reset to prevent drift)
- **Drone** = holds position

---

## Troubleshooting

### MAVLink Not Connecting

1. Check UART wiring (TX→RX, RX→TX, GND→GND)
2. Verify baud rate matches flight controller (57600)
3. Check `/dev/ttyAMA0` exists
4. Add user to dialout group: `sudo usermod -a -G dialout $USER`

### Tracking Lost Frequently

1. Reduce camera motion (vibration dampening)
2. Increase dead zone size (`centering_radius`)
3. Try different tracker (CSRT is more robust)
4. Improve lighting conditions

### FPS Too Low

1. Reduce camera resolution
2. Use faster tracker (CSRT > SiamFC++ > VitTracker)
3. Disable frame saving in code
4. Check CPU temperature (throttling)

### Drone Not Responding to Commands

1. Verify mode is **GUIDED_NOGPS** (mode 20)
2. Check "RC: ACTIVE" in status bar
3. Ensure drone is armed
4. Check `GUID_OPTIONS` parameter in ArduPilot

### Pitch/Yaw Always at Maximum

1. Check dead zone size (may be too small)
2. Verify PID gains are reasonable
3. Ensure `error_to_angle_scale` is correct
4. Check if integral is accumulating excessively

---

## Key Files Reference

| File | Purpose |
|------|---------|
| `config/config.txt` | Main configuration |
| `src/navigation_unit.cpp` | PID control logic |
| `src/application.cpp` | Main threads |
| `src/mavlink.cpp` | MAVLink communication |
| `include/navigation_unit.h` | PID gains |

---

## Support

For issues or feature requests, check the documentation in the `docs/` folder or review the codebase comments.

**Version**: December 2024  
**Platform**: Raspberry Pi 5 + ArduPilot
