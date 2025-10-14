# Color Detection Feature - Implementation Summary

## Overview
Added color detection as a new detection mode (model_type=3) that can be used to start the tracker. The color detector uses HSV color space to identify objects by color and provides bounding boxes compatible with the tracking system.

## Implementation Details

### 1. ColorDetector Class
**Files Created:**
- `include/detection/color_detector.h`
- `src/detection/color_detector.cpp`

**Features:**
- HSV-based color detection with configurable color ranges
- Morphological operations to reduce noise (erosion/dilation)
- Contour detection with area filtering
- Returns `model::Detection` objects compatible with existing tracking system
- Supports multiple colors simultaneously: red, blue, green, yellow, orange, purple

**Key Methods:**
- `detect(const cv::Mat& frame)` - Detects colored objects and returns bounding boxes
- `applyMorphology(cv::Mat& mask)` - Cleans up detection mask
- `createDetectionsFromMask()` - Converts contours to Detection objects

### 2. ModelManager Extensions
**Files Modified:**
- `include/model_manager.h`
- `src/model_manager.cpp`

**Changes:**
- Added `COLOR_DETECTION = 3` to ModelType enum (already existed)
- Added `ColorDetectionConfig` struct to ModelConfig
- Added `colorDetector_` member variable
- Implemented `initializeColorDetection()` method with predefined HSV ranges
- Updated `detect()` to route to color detector when in color detection mode

**Supported Colors & HSV Ranges:**
- Red: H:0-10 and H:170-180, S:100-255, V:100-255
- Blue: H:100-130, S:100-255, V:100-255
- Green: H:40-80, S:50-255, V:50-255
- Yellow: H:20-35, S:100-255, V:100-255
- Orange: H:10-20, S:100-255, V:100-255
- Purple: H:130-160, S:50-255, V:50-255

### 3. Application Integration
**Files Modified:**
- `src/application.cpp`

**Changes:**
- Added `<sstream>` include for string parsing
- Extended `initializeModels()` to handle model_type=3
- Parses comma-separated color list from config
- Reads min_area and max_area from config
- Creates ModelConfig with colorConfig settings

### 4. Configuration
**Files Modified:**
- `config/config.txt`

**New Section Added:**
```ini
[color_detection]
target_colors=red,blue
min_area=500
max_area=50000
```

**Updated:**
- `detection_model.model_type` comment now includes: "3=color detection"

### 5. Build System
**Files Modified:**
- `ai.pro`

**Changes:**
- Added `src/detection/color_detector.cpp` to SOURCES list

## Usage

### To Enable Color Detection:

1. **Edit config.txt:**
```ini
[detection_model]
model_type=3  # Enable color detection
```

2. **Configure colors to detect:**
```ini
[color_detection]
# Detect red and blue objects
target_colors=red,blue
min_area=500
max_area=50000
```

3. **Rebuild the application:**
   - In Qt Creator: Clean All → Run qmake → Build
   - Or command line: `qmake && make`

4. **Run the application:**
   - Color detection will work with tracking mode (operation_mode=0)
   - Detected objects will be tracked using VitTracker or SiamFCPP
   - MAVLink will receive position updates for tracked colored objects

### Configuration Options:

**target_colors:**
- Comma-separated list of colors
- Supported: red, blue, green, yellow, orange, purple
- Example: `target_colors=red,blue,green`

**min_area:**
- Minimum contour area in pixels squared
- Filters out small noise detections
- Default: 500

**max_area:**
- Maximum contour area in pixels squared
- Prevents detecting entire frame as object
- Default: 50000

## How It Works

1. **Frame Capture:** Camera captures BGR frame
2. **Color Space Conversion:** Frame converted from BGR to HSV
3. **Color Masking:** For each target color:
   - Apply cv::inRange() with HSV bounds
   - Creates binary mask of matching pixels
4. **Noise Reduction:** Morphological operations (opening + closing)
5. **Contour Detection:** Find external contours in mask
6. **Area Filtering:** Keep only contours within min/max area
7. **Bounding Box Creation:** Get bounding rectangle for each contour
8. **Confidence Calculation:** Based on how "filled" the box is
9. **Return Detections:** List of Detection objects with bbox, class, confidence
10. **Tracking:** Detections fed to tracker system (same as YOLO detections)

## Advantages

- **No AI Model Required:** No ONNX model needed, pure OpenCV
- **Fast Performance:** HSV filtering is very fast
- **Low Memory:** No neural network weights to load
- **Real-time:** Suitable for high-speed tracking
- **Adjustable:** Easy to tune HSV ranges for different lighting conditions

## Limitations

- **Lighting Sensitive:** HSV values change with lighting conditions
- **No Object Classification:** Only detects colors, not object types
- **Color Confusion:** May detect wrong objects with similar colors
- **Background Noise:** Needs clean background for best results

## Testing Recommendations

1. Start with one color (e.g., red)
2. Use a bright, solid-colored object
3. Test in good lighting conditions
4. Adjust min_area/max_area based on object size
5. Fine-tune HSV ranges if needed (requires code modification)

## Integration with Existing Features

✅ **Compatible with:**
- Detection + Tracking mode (operation_mode=0)
- Detection only mode (operation_mode=1)
- VitTracker and SiamFCPP trackers
- MAVLink communication
- Path visualization
- All selection strategies
- Frame buffer management

✅ **Works like YOLO detection:**
- Returns same Detection structure
- Uses same detection manager
- Integrates with same tracking pipeline
- No changes needed to other components

## Files Summary

**Created (2 files):**
- include/detection/color_detector.h
- src/detection/color_detector.cpp

**Modified (5 files):**
- include/model_manager.h
- src/model_manager.cpp
- src/application.cpp
- config/config.txt
- ai.pro

**Total Lines Added:** ~350 lines

## Next Steps

1. Build the project on Raspberry Pi 5
2. Test with a solid red or blue object
3. Verify tracking works with color detection
4. Adjust HSV ranges if needed for your lighting
5. Tune min/max area for your use case
