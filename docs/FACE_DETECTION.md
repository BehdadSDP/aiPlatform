# Face Detection with YOLOv10n-face

## Overview

This AI platform now supports specialized face detection using the YOLOv10n-face.onnx model from the [yolo-face repository](https://github.com/akanametov/yolo-face). This model is optimized for accurate face detection with good performance on edge devices.

## Features

✅ **YOLOv10n-face ONNX Integration**: Uses the state-of-the-art YOLOv10 architecture trained specifically for face detection  
✅ **Real-time Face Tracking**: Continuous face tracking in operation mode 0  
✅ **Hazard Zone Monitoring**: Face-specific hazard zones for privacy or security applications  
✅ **High Performance**: Optimized for Raspberry Pi 5 with ARM64 architecture  
✅ **Easy Configuration**: Simple model type switching  

## Installation

### 1. Download the Model

Based on the [yolo-face repository](https://github.com/akanametov/yolo-face), you have several options:

**Option A: Download pre-converted ONNX model**
```bash
# Visit the repository and download yolov10n-face.onnx
# Place it in: /home/pi5/shared_folder/aiPlatform/models/yolov10n-face.onnx
```

**Option B: Convert from PyTorch (.pt) format**
```bash
# Install ultralytics
pip install ultralytics

# Download the .pt model from the repository
# Then convert to ONNX
yolo export model=yolov10n-face.pt format=onnx

# Move the resulting .onnx file to your models directory
mv yolov10n-face.onnx /home/pi5/shared_folder/aiPlatform/models/
```

**Option C: Use the download script**
```bash
# Run the provided script for guidance
./scripts/download_face_model.sh
```

### 2. Verify Model Files

Ensure you have these files:
```
models/
├── yolov10n-face.onnx     # Face detection model
└── face.names             # Class names (already created)
```

### 3. Configure for Face Detection

Set `model_type=2` in your `config/config.txt`:

```ini
[detection_model]
model_type=2  # Face detection mode

# Face detection model paths
face_model_path=/home/pi5/shared_folder/aiPlatform/models/yolov10n-face.onnx
face_names_path=/home/pi5/shared_folder/aiPlatform/models/face.names
```

## Usage Examples

### Basic Face Detection
```bash
# Set model_type=2 in config.txt
./ai
```

### Face Detection with Tracking
```bash
# Ensure operation_mode=0 in config.txt for tracking
# The system will detect faces and track them continuously
./ai
```

### Face Detection with Hazard Zones

Configure privacy zones in `config.txt`:
```ini
[hazard_zones]
alarm_enabled=1
visual_alarm=1

# Privacy zone example
zone1=300,200,200,200
zone1_name=Privacy Zone
zone1_active=1
```

## Model Specifications

**YOLOv10n-face Model Details:**
- **Architecture**: YOLOv10 nano optimized for face detection
- **Input Size**: 640x640 (configurable)
- **Classes**: 1 (face)
- **Format**: ONNX
- **Performance**: ~30-60 FPS on Raspberry Pi 5
- **Accuracy**: Optimized for various face sizes and orientations

## Configuration Options

### Model Selection
```ini
# Model types available:
# 0 = COCO General Detection (80 classes)
# 1 = Helmet Detection (safety)
# 2 = Face Detection (YOLOv10n-face)
model_type=2
```

### Face-Specific Settings
```ini
[general]
target_class_id=0  # Face class (0 = face)

[detection]
interval=100  # Fast detection interval for faces
selection_strategy=0  # Highest confidence face

[tracking]
mode=0    # Enable tracking
interval=1  # Continuous tracking
```

### Hazard Zones for Faces
```ini
[hazard_zones]
# Example: Privacy zones, restricted areas, etc.
zone1=100,100,200,200
zone1_name=No Photography Zone
zone1_active=1

zone2=400,300,150,150  
zone2_name=Secure Area
zone2_active=1
```

## Use Cases

### 1. **Privacy Monitoring**
- Detect faces in restricted camera zones
- Alert when faces appear in private areas
- Real-time privacy violation detection

### 2. **Security Applications**
- Face detection in secure areas
- Continuous tracking of detected faces
- Integration with existing security systems

### 3. **Attendance Systems**
- Face detection for presence monitoring
- Tracking face movement in defined areas
- Automated logging and alerts

### 4. **Safety Monitoring**
- Face detection in hazardous zones
- Personal protective equipment compliance
- Emergency response triggers

## Performance Optimization

**For Raspberry Pi 5:**
- Model uses ONNX Runtime backend when available
- Optimized for ARM64 architecture
- Automatic fallback to OpenCV DNN backend
- Performance monitoring with timing logs

**Expected Performance:**
- **Detection**: 20-50ms per frame
- **Total FPS**: 30-60 FPS (depending on resolution)
- **Memory Usage**: ~200-400MB
- **CPU Usage**: 40-70% (single core)

## Troubleshooting

### Model Loading Issues
```bash
# Check if model file exists and is accessible
ls -la models/yolov10n-face.onnx

# Verify file permissions
chmod 644 models/yolov10n-face.onnx
```

### Performance Issues
```bash
# Monitor resource usage
./ai  # Watch for performance logs every 30 detections
```

### Detection Quality
- Ensure good lighting conditions
- Use higher resolution if needed (640x640 recommended)
- Adjust confidence thresholds in the model if necessary

## Integration with Existing Features

✅ **Hazard Zone Manager**: Fully compatible  
✅ **Tracking System**: Works with VitTracker and SiamFCPP  
✅ **Detection Visualizer**: Shows face detection boxes  
✅ **Resource Monitor**: Tracks performance metrics  
✅ **Configuration System**: Hot-swappable model types  

## References

- **Source Repository**: [yolo-face by akanametov](https://github.com/akanametov/yolo-face)
- **YOLOv10 Paper**: [YOLOv10: Real-Time End-to-End Object Detection](https://arxiv.org/abs/2405.14458)
- **Ultralytics Documentation**: [docs.ultralytics.com](https://docs.ultralytics.com)

## License

The YOLOv10n-face model follows the GPL-3.0 license as specified in the [yolo-face repository](https://github.com/akanametov/yolo-face). 