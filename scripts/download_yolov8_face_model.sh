#!/bin/bash

# Download YOLOv8n-face.onnx model for face detection
# Based on: https://github.com/akanametov/yolo-face
# YOLOv8 is more compatible with OpenCV DNN than YOLOv10

MODELS_DIR="/home/pi5/shared_folder/aiPlatform/models"
MODEL_NAME="yolov8n-face.onnx"

# Create models directory if it doesn't exist
mkdir -p "$MODELS_DIR"

echo "📥 Downloading YOLOv8n-face.onnx model..."
echo "Source: https://github.com/akanametov/yolo-face"
echo "⚠️  Using YOLOv8 instead of YOLOv10 for better OpenCV compatibility"

# Check if model already exists
if [ -f "$MODELS_DIR/$MODEL_NAME" ]; then
    echo "✅ $MODEL_NAME already exists in $MODELS_DIR"
    ls -lh "$MODELS_DIR/$MODEL_NAME"
    exit 0
fi

echo ""
echo "🔗 To download the YOLOv8n-face.onnx model:"
echo ""
echo "Option 1: Manual download from repository"
echo "  1. Visit: https://github.com/akanametov/yolo-face"
echo "  2. Download the yolov8n-face.onnx model"
echo "  3. Place it in: $MODELS_DIR/$MODEL_NAME"
echo ""

echo "Option 2: Convert from PyTorch format"
echo "  pip install ultralytics"
echo "  # Download yolov8n-face.pt from the repository"
echo "  yolo export model=yolov8n-face.pt format=onnx"
echo "  mv yolov8n-face.onnx $MODELS_DIR/"
echo ""

echo "Option 3: Try direct download (if available)"
echo "  # Check the repository for direct ONNX download links"
echo ""

# Attempt to download if a direct link is available (placeholder)
echo "🔍 Checking for available download links..."

# You can add direct download commands here if URLs are available
# Example:
# wget -O "$MODELS_DIR/$MODEL_NAME" "https://github.com/akanametov/yolo-face/releases/download/v1.0/yolov8n-face.onnx"

echo ""
if [ ! -f "$MODELS_DIR/$MODEL_NAME" ]; then
    echo "❌ $MODEL_NAME not found in $MODELS_DIR"
    echo ""
    echo "📝 Manual steps required:"
    echo "  1. Visit: https://github.com/akanametov/yolo-face"
    echo "  2. Download or convert yolov8n-face.onnx"
    echo "  3. Copy to: $MODELS_DIR/$MODEL_NAME"
    echo ""
    echo "🔧 Alternative: Use existing COCO model temporarily:"
    echo "  Set model_type=0 in config.txt to use general object detection"
fi

echo ""
echo "🚀 Once downloaded, ensure model_type=2 in config/config.txt"
echo "🎯 YOLOv8n-face should work better with OpenCV DNN than YOLOv10" 