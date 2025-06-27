# AI Platform - Project Overview

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                                AI PLATFORM ARCHITECTURE                              │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   INPUT SOURCE  │    │   CONFIGURATION │    │   MAIN CONTROL  │    │   RESOURCE      │
│                 │    │                 │    │                 │    │   MONITORING    │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│ • CameraHandler │    │ • Config Utils  │    │ • Main Thread   │    │ • Resource      │
│ • VideoHandler  │    │ • Config Files  │    │ • Signal Handler│    │   Monitor       │
│ • Frame Buffer  │    │ • Model Paths   │    │ • Thread Mgmt   │    │ • Performance   │
│   Manager       │    │ • Parameters    │    │ • Error Handling│    │   Logging       │
└─────────────────┘    └─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │                       │
         ▼                       ▼                       ▼                       ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                              CORE PROCESSING PIPELINE                               │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   DETECTION     │    │   TRACKING      │    │   VISUALIZATION │    │   SAFETY        │
│   SYSTEM        │    │   SYSTEM        │    │   SYSTEM        │    │   MONITORING    │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│ • Model Manager │    │ • Tracker       │    │ • Detection     │    │ • Hazard Zone   │
│ • YOLO Detector │    │   Factory       │    │   Visualizer    │    │   Manager       │
│ • Detection     │    │ • VitTracker    │    │ • Path Tracking │    │ • Traffic       │
│   Manager       │    │ • SiamFCPP      │    │ • Real-time     │    │   Intensity     │
│ • Detection     │    │ • Tracker       │    │   Display       │    │   Manager       │
│   Processor     │    │   Manager       │    │ • Performance   │    │ • Alarm System  │
└─────────────────┘    └─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │                       │
         ▼                       ▼                       ▼                       ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                              OUTPUT & COMMUNICATION                                 │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   CONTROL UNIT  │    │   LOGGING       │    │   MAVLINK       │    │   USER          │
│                 │    │   SYSTEM        │    │   OUTPUT        │    │   INTERFACE     │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│ • Mode Control  │    │ • Resource      │    │ • Heartbeat     │    │ • Console       │
│ • Interval      │    │   Usage Logs    │    │   Messages      │    │   Output        │
│   Management    │    │ • Performance   │    │ • Status        │    │ • Error         │
│ • Thread        │    │   Metrics       │    │   Updates       │    │   Messages      │
│   Coordination  │    │ • System Events │    │ • Data          │    │ • Progress      │
└─────────────────┘    └─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Data Flow Diagram

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Camera    │────▶│ Frame Buffer│────▶│ Detection   │────▶│ Detection   │
│   / Video   │     │   Manager   │     │   Manager   │     │   Results   │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
       │                    │                    │                    │
       ▼                    ▼                    ▼                    ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Config    │────▶│ Model       │────▶│ YOLO        │────▶│ Visualization│
│   Loading   │     │   Manager   │     │   Detector  │     │   & Display │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
       │                    │                    │                    │
       ▼                    ▼                    ▼                    ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Control   │────▶│ Tracker     │────▶│ Tracking    │────▶│ Safety      │
│   Unit      │     │   Factory   │     │   Results   │     │   Monitoring│
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
```

## Component Details

### 1. Input Layer
- **CameraHandler**: Manages libcamera interface for Raspberry Pi camera
- **VideoHandler**: Handles video file input processing
- **FrameBufferManager**: Manages frame buffering and memory allocation

### 2. Configuration System
- **ConfigUtils**: Loads and parses configuration files
- **Model Paths**: Manages ONNX model file paths
- **Parameters**: Handles system configuration parameters

### 3. Detection System
- **ModelManager**: Manages different AI models (YOLO variants)
- **YOLODetector**: Implements YOLO object detection
- **DetectionManager**: Coordinates detection operations
- **DetectionProcessor**: Processes detection results

### 4. Tracking System
- **TrackerFactory**: Creates different tracker instances
- **VitTracker**: Vision Transformer based tracker
- **SiamFCPP**: Siamese Fully Convolutional tracker
- **TrackerManager**: Manages tracking operations

### 5. Safety Monitoring
- **HazardZoneManager**: Monitors predefined hazard zones
- **TrafficIntensityManager**: Analyzes traffic density
- **Alarm System**: Provides visual/audio alerts

### 6. Visualization
- **DetectionVisualizer**: Renders detection results
- **Path Tracking**: Visualizes object movement paths
- **Real-time Display**: Shows live processing results

### 7. Control & Monitoring
- **ControlUnit**: Coordinates system operations
- **ResourceMonitor**: Monitors system performance
- **Thread Management**: Manages concurrent processing

## Supported Models

### Detection Models
1. **Vehicle Detection**: YOLOv8 for vehicle detection
2. **Helmet Detection**: YOLOv8n for helmet detection
3. **Face Detection**: YOLOv11n for face detection

### Tracking Models
1. **VitTracker**: Vision Transformer based tracking
2. **SiamFCPP**: Siamese Fully Convolutional tracking

## Operation Modes

### Mode 0: Detection + Tracking
- Performs object detection
- Initializes tracking on detected objects
- Provides continuous object tracking

### Mode 1: Detection Only
- Performs object detection only
- No tracking functionality
- Optimized for detection tasks

## Key Features

1. **Multi-Model Support**: Supports different YOLO variants
2. **Real-time Processing**: Optimized for live video processing
3. **Safety Monitoring**: Hazard zone and traffic intensity analysis
4. **Resource Management**: Performance monitoring and optimization
5. **Configurable**: Extensive configuration options
6. **Thread-Safe**: Multi-threaded architecture
7. **Error Handling**: Robust error handling and recovery

## File Structure

```
aiPlatform/
├── src/                    # Source files
│   ├── main.cpp           # Main application entry
│   ├── camera_handler.cpp # Camera interface
│   ├── detection_*.cpp    # Detection system
│   ├── tracker_*.cpp      # Tracking system
│   └── ...
├── include/               # Header files
│   ├── camera_handler.h
│   ├── detection_*.h
│   ├── tracker_*.h
│   └── ...
├── config/               # Configuration files
│   └── config.txt
├── models/               # AI model files
│   ├── *.onnx           # ONNX models
│   └── *.names          # Class name files
├── logs/                # Log files
├── test/                # Test files
└── docs/                # Documentation
```

## Dependencies

- **OpenCV**: Computer vision operations
- **libcamera**: Camera interface for Raspberry Pi
- **Qt**: Build system and utilities
- **ONNX Runtime**: Neural network inference
- **Standard C++**: Core functionality

## Performance Characteristics

- **Frame Rate**: Configurable (typically 20-30 FPS)
- **Resolution**: Configurable (640x640 default)
- **Memory Usage**: Optimized buffer management
- **CPU Usage**: Multi-threaded processing
- **Latency**: Real-time processing capabilities 