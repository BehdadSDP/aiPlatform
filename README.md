# AI Platform - Object Detection and Tracking System

A sophisticated computer vision application that combines YOLO object detection with advanced tracking algorithms (VitTracker and SiamFCPP) for real-time target tracking. The system supports both camera input and video file input.

## Features

- **Multi-Input Support**: Camera or video file input
- **Multi-Model Detection**: YOLO-based detection with support for:
  - General object detection (COCO dataset)
  - **Helmet/Safety Detection** for workplace safety monitoring
- **Advanced Tracking**: VitTracker and SiamFCPP tracker implementations
- **Real-time Processing**: Multi-threaded architecture for optimal performance
- **Flexible Selection Strategies**: Multiple detection selection methods
- **Safety Monitoring**: Visual indicators for helmet compliance
- **Resource Monitoring**: Built-in system resource monitoring
- **Configurable Parameters**: External configuration file support

## Architecture

### Core Components

- **Input Handlers**: 
  - `CameraHandler`: libcamera integration for Raspberry Pi
  - `VideoHandler`: OpenCV-based video file processing
- **Detection**: YOLO object detection with ONNX model support
- **Tracking**: Pluggable tracker interface supporting multiple algorithms
- **Frame Management**: Circular buffer for efficient frame handling
- **Control Unit**: Coordinates detection and tracking operations

### Design Patterns Used

- **Strategy Pattern**: Detection selection strategies
- **Adapter Pattern**: Tracker interface adapters
- **Singleton Pattern**: Resource monitor and frame buffer manager
- **Factory Pattern**: Strategy creation

## Configuration

### Input Types

The system supports two input types configured via `config/config.txt`:

#### Camera Input (input_type=0)
```ini
[input]
input_type=0

[camera]
resolution_index=4
width=640
height=640
frame_rate=20
```

#### Video Input (input_type=1)
```ini
[input]
input_type=1
video_path=/path/to/your/video.mp4

# Camera settings are ignored when using video input
```

### Detection Model Settings
```ini
[detection_model]
# Model type: 0=COCO general detection, 1=helmet detection
model_type=0

# COCO model paths (used when model_type=0)
yolo_model_path=/home/pi5/shared_folder/aiPlatform/models/yolov12n.onnx
coco_names_path=/home/pi5/shared_folder/aiPlatform/models/coco.names

# Helmet model paths (used when model_type=1)
helmet_model_path=/home/pi5/shared_folder/aiPlatform/models/helmet_yolov8n.onnx
helmet_names_path=/home/pi5/shared_folder/aiPlatform/models/helmet.names
```

### Detection Settings
```ini
[detection]
mode=1
interval=8000
# Selection strategy: 0=highest confidence, 1=upper box, 2=lower box, 3=rightmost, 4=leftmost
selection_strategy=4
```

### Tracking Settings
```ini
[tracking]
mode=0
interval=1
# Tracker type: 0 for VitTracker, 1 for SiamFCPP tracker
tracker_type=1
```

## Building

### Prerequisites

- Qt 5/6 development libraries
- OpenCV 4.x with DNN support
- libcamera (for camera input)
- C++17 compiler

### Build Instructions

```bash
# Navigate to project directory
cd aiPlatform

# Generate Makefile
qmake ai.pro

# Build
make -j4
```

## Usage

### Running with Camera Input

1. Set `input_type=0` in `config/config.txt`
2. Configure camera settings as needed
3. Run the application:
```bash
./ai
```

### Running with Video Input

1. Set `input_type=1` in `config/config.txt`
2. Set `video_path` to your video file location
3. Run the application:
```bash
./ai
```

The video will loop continuously for continuous processing.

## Supported Video Formats

The system supports all video formats supported by OpenCV, including:
- MP4 (H.264, H.265)
- AVI
- MOV
- MKV
- WebM

## Model Files

Place the following model files in the `models/` directory:

### Detection Models
- **General YOLO Models**: `yolov12n.onnx`, `yolov12m.onnx`
- **Helmet Detection Model**: `helmet_yolov8n.onnx` (download with script)
- **Class Names**: `coco.names`, `helmet.names`

### Tracking Models
- **SiamFCPP Models**: `siamfc_pp_tracker_feature.onnx`, `siamfc_pp_tracking.onnx`
- **VitTracker Model**: `vittracker.onnx`

### Download Helmet Detection Model

Use the provided script to download the helmet detection model:

```bash
cd scripts
./download_helmet_model.sh
```

Or download manually from: [Safety-Helmet-Detection Repository](https://github.com/jomarkow/Safety-Helmet-Detection)

## Visualization

The system provides real-time visualization windows:

- **Detection View**: Shows detected objects with bounding boxes and confidence scores
- **Tracker View**: Shows tracking results with target highlighting

## Performance Monitoring

Resource usage is automatically logged to `logs/resource_usage.csv` including:
- CPU usage
- Memory usage
- System temperature
- Disk I/O statistics
- Network statistics

## Configuration Options

### Detection Selection Strategies

- `0`: Highest confidence detection
- `1`: Upper bounding box (topmost detection)
- `2`: Lower bounding box (bottommost detection)
- `3`: Rightmost bounding box
- `4`: Leftmost bounding box

### Tracker Types

- `0`: VitTracker (Vision Transformer-based)
- `1`: SiamFCPP (Siamese Fully Convolutional++)

## Error Handling

The system includes comprehensive error handling for:
- Invalid video file paths
- Camera initialization failures
- Model loading errors
- Configuration validation

## Example Workflows

### Helmet Safety Monitoring

1. Download the helmet detection model:
```bash
cd scripts && ./download_helmet_model.sh
```

2. Configure for helmet detection in `config.txt`:
```ini
[detection_model]
model_type=1

[input]
input_type=1  # or 0 for camera
video_path=/path/to/construction_site_video.mp4
```

3. Run the application for safety monitoring:
```bash
./ai
```

The system will:
- ✅ **Green boxes**: Workers wearing helmets (SAFE)
- ❌ **Red boxes**: Workers without helmets (VIOLATION)
- 🟠 **Orange boxes**: People detected

### Object Tracking from Video

1. Place your video file in the `test/` directory
2. Update `config.txt`:
```ini
[input]
input_type=1
video_path=/home/pi5/shared_folder/aiPlatform/test/your_video.mp4
```
3. Run the application
4. The system will detect objects and track them throughout the video

### Real-time Camera Tracking

1. Ensure camera is connected
2. Set `input_type=0` in config
3. Configure desired resolution and frame rate
4. Run the application for real-time processing

## Troubleshooting

### Common Issues

- **Build Errors**: Ensure all dependencies are installed and paths are correct
- **Video Not Loading**: Check file path and format compatibility
- **Poor Detection**: Adjust confidence thresholds or try different YOLO models
- **Tracking Loss**: Consider switching tracker types or adjusting selection strategy

## Future Enhancements

- [ ] Support for multiple simultaneous targets
- [ ] Custom model training integration
- [ ] Network streaming capabilities
- [ ] Advanced tracking algorithms
- [ ] Mobile device support 