#!/bin/bash

# Script to download helmet detection YOLO model

MODELS_DIR="../models"
HELMET_MODEL_URL="https://github.com/jomarkow/Safety-Helmet-Detection/raw/main/yolov8n.onnx"
HELMET_MODEL_NAME="helmet_yolov8n.onnx"

echo "Downloading helmet detection YOLO model..."

# Create models directory if it doesn't exist
mkdir -p "$MODELS_DIR"

# Download the helmet detection model
echo "Downloading $HELMET_MODEL_NAME from GitHub..."
wget -O "$MODELS_DIR/$HELMET_MODEL_NAME" "$HELMET_MODEL_URL"

if [ $? -eq 0 ]; then
    echo "✅ Successfully downloaded helmet detection model to: $MODELS_DIR/$HELMET_MODEL_NAME"
    echo "📏 Model size: $(ls -lh "$MODELS_DIR/$HELMET_MODEL_NAME" | awk '{print $5}')"
else
    echo "❌ Failed to download helmet detection model"
    exit 1
fi

echo ""
echo "🎯 To use helmet detection:"
echo "1. Set model_type=1 in config/config.txt"
echo "2. Make sure helmet_model_path points to: $MODELS_DIR/$HELMET_MODEL_NAME"
echo "3. Run your application"
echo ""
echo "Classes detected by this model:"
echo "- helmet"
echo "- no-helmet"
echo "- person"
echo "- hardhat"
echo "- head" 