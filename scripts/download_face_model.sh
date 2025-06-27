#!/bin/bash

# Download YOLOv10n-face.onnx model for face detection
# Based on: https://github.com/akanametov/yolo-face

MODELS_DIR="/home/pi5/shared_folder/aiPlatform/models"
MODEL_NAME="yolov10n-face.onnx"

# Create models directory if it doesn't exist
mkdir -p "$MODELS_DIR"

echo "📥 Downloading YOLOv10n-face.onnx model..."
echo "Source: https://github.com/akanametov/yolo-face"

# Download the model (you'll need to get the direct download link)
# For now, we'll provide instructions for manual download
echo ""
echo "🔗 To download the YOLOv10n-face.onnx model:"
echo ""
echo "1. Visit: https://github.com/akanametov/yolo-face"
echo "2. Download the yolov10n-face.onnx model"
echo "3. Place it in: $MODELS_DIR/$MODEL_NAME"
echo ""
echo "Alternative: If you have the model locally, copy it:"
echo "cp /path/to/your/yolov10n-face.onnx $MODELS_DIR/"
echo ""

# Check if model already exists
if [ -f "$MODELS_DIR/$MODEL_NAME" ]; then
    echo "✅ $MODEL_NAME already exists in $MODELS_DIR"
    ls -lh "$MODELS_DIR/$MODEL_NAME"
else
    echo "❌ $MODEL_NAME not found in $MODELS_DIR"
    echo ""
    echo "📝 You can also convert from PyTorch format:"
    echo "pip install ultralytics"
    echo "yolo export model=yolov10n-face.pt format=onnx"
fi

echo ""
echo "🚀 Once downloaded, set model_type=2 in config/config.txt to use face detection" 