# AI Platform Qt GUI

## Overview

A modern Qt-based graphical user interface for the AI Platform object detection and tracking system, optimized for Raspberry Pi 5.

## Features

### Control Panel (Left)
- **Input Source Selection**
  - Camera (default)
  - Video file with browse dialog
  
- **Detection Model Selection**
  - Vehicle Detection
  - Helmet Detection
  - Face Detection
  - Color Detection
  
- **Operation Mode**
  - Detect + Track (full pipeline)
  - Detect Only (detection without tracking)
  
- **Tracker Settings**
  - VitTracker (default)
  - SiamFC++
  - CSRT
  - Show tracking path option
  
- **MAVLink Control**
  - Enable/disable MAVLink
  - Connection status
  - Flight mode display
  - Altitude monitoring
  - Battery level with color-coded progress bar
  
- **Control Buttons**
  - Start: Begin processing
  - Stop: Stop processing
  - Pause/Resume: Pause/resume processing

### Video Display (Center)
- Real-time video feed with detections and tracking overlays
- Automatic scaling to fit window
- Maintains aspect ratio

### Status Panel (Right)
- **Status Information**
  - Current system status
  - FPS counter
  - Detection count
  - Tracking status with class name and confidence
  
- **System Resources**
  - CPU usage with progress bar
  - Memory usage with progress bar
  
- **Log Messages**
  - Real-time log display
  - Auto-scrolling
  - Limited to 1000 lines (auto-cleanup)

## Running the Application

### GUI Mode (Default)
```bash
cd /home/pi5/shared_folder/aiPlatform
./ai
```

### Console Mode (Headless)
```bash
./ai --no-gui
# or
./ai -ng
```

### With Custom Config
```bash
./ai --config /path/to/config.txt
# or
./ai -c /path/to/config.txt
```

### Combined Options
```bash
./ai --no-gui --config /path/to/config.txt
```

## Building

Make sure Qt widgets are enabled in your `.pro` file:

```qmake
QT += core widgets
CONFIG += c++17
```

Build the project:
```bash
cd /home/pi5/shared_folder/aiPlatform
qmake
make -j4
```

## UI Theme

The interface uses a modern dark theme optimized for:
- Reduced eye strain during long operations
- Clear visibility in various lighting conditions
- Professional appearance
- Raspberry Pi 5 display compatibility

### Color Scheme
- Background: Dark gray (#353535, #404040)
- Text: Light gray (#e0e0e0)
- Borders: Medium gray (#555, #666)
- Accent: Blue (#4a90d9)
- Start button: Green (#2d5f2d)
- Stop button: Red (#5f2d2d)
- Battery warning: Orange (#ff9900), Red (#ff3333)

## Window Layout

```
┌────────────────────────────────────────────────────────────────┐
│  File   Help                                          [□][×]    │
├────────┬──────────────────────────────────┬────────────────────┤
│        │                                  │  Status            │
│ Input  │                                  │  ├─ Status: Ready  │
│ Source │                                  │  ├─ FPS: 0.0       │
│        │                                  │  ├─ Detections: 0  │
│ Model  │      Video Display Area          │  └─ Tracking: --   │
│ Select │      (640x480 minimum)           │                    │
│        │                                  │  System Resources  │
│ Tracker│                                  │  ├─ CPU: [====]    │
│ Config │                                  │  └─ Memory: [==]   │
│        │                                  │                    │
│ MAVLink│                                  │  Log Messages      │
│ Control│                                  │  ┌───────────────┐ │
│        │                                  │  │               │ │
│ [Start]│                                  │  │  Log output   │ │
│ [Stop] │                                  │  │  ...          │ │
│ [Pause]│                                  │  └───────────────┘ │
└────────┴──────────────────────────────────┴────────────────────┘
```

## Integration with Application

The MainWindow class provides slots for updating the UI from the Application class:

```cpp
// Update video frame
window.updateVideoFrame(frame);

// Update statistics
window.updateFPS(30.5);
window.updateDetectionStats(5);
window.updateTrackingInfo(true, "car", 0.95f);

// Update MAVLink status
window.updateMAVLinkStatus(true, flightMode, armed, altitude, battery);

// Update system resources
window.updateResourceStats(45.2f, 62.3f);

// Log messages
window.logMessage("Detection started");
```

## Performance Considerations

### Raspberry Pi 5 Optimization
- Video frame updates use efficient Qt pixmap conversion
- UI refresh rate: 10Hz (100ms timer)
- Log buffer limited to 1000 lines
- Smooth scaling for video display
- Minimal memory footprint

### Thread Safety
The MainWindow is designed to be called from the main Qt thread. If updating from worker threads, use:

```cpp
QMetaObject::invokeMethod(&window, "updateVideoFrame", 
    Qt::QueuedConnection, Q_ARG(cv::Mat, frame));
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Ctrl+S | Save Configuration |
| Ctrl+Q | Quit Application |

## Known Limitations

1. **Application Integration**: The `startApplication()` method is a placeholder. Full integration requires:
   - Modifying `Application` class to work with Qt event loop
   - Implementing proper thread management
   - Adding signals/slots for communication

2. **Configuration**: Currently loads/saves basic settings. Extend for full config support.

3. **Video Display**: Frame updates must be called from your processing pipeline.

## Future Enhancements

- [ ] Real-time parameter adjustment (PID gains, detection threshold, etc.)
- [ ] Recording functionality (save video with overlays)
- [ ] Statistics graphs (FPS over time, CPU usage trends)
- [ ] Manual object selection for tracking
- [ ] ROI (Region of Interest) configuration
- [ ] Multiple camera support
- [ ] Snapshot capture
- [ ] Configuration profiles (quick switching between setups)

## Troubleshooting

### Black video display
- Check if `updateVideoFrame()` is being called
- Verify OpenCV frame is valid (not empty)
- Check color space conversion (BGR→RGB)

### UI not responding
- Ensure Qt event loop is running (`qapp.exec()`)
- Check for blocking operations in main thread
- Use worker threads for heavy processing

### High CPU usage
- Reduce UI refresh rate if needed
- Optimize video frame scaling
- Limit log message frequency

### Display issues on Pi 5
- Check Qt platform plugins: `export QT_QPA_PLATFORM=xcb`
- Verify X11 is running
- Try Wayland: `export QT_QPA_PLATFORM=wayland`

## Dependencies

- Qt 6.4.2 or later (Core, Widgets)
- OpenCV 4.x (for Mat→QPixmap conversion)
- C++17 compiler
- Raspberry Pi OS with desktop environment

## License

Part of AI Platform project.
