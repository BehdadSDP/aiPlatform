#!/usr/bin/env python3
"""
Convert YOLOv10 face model to OpenCV DNN compatible format
Removes TopK and other unsupported operators
"""

import sys
import argparse
from pathlib import Path

def check_dependencies():
    """Check if required packages are installed"""
    try:
        import onnx
        import onnxsim
        print("✅ Required packages found: onnx, onnxsim")
        return True
    except ImportError as e:
        print(f"❌ Missing required packages: {e}")
        print("\n📦 Install required packages:")
        print("pip install onnx onnxsim")
        return False

def simplify_model(input_path, output_path):
    """Simplify ONNX model to remove complex operators"""
    try:
        import onnx
        import onnxsim
        
        print(f"📥 Loading model: {input_path}")
        model = onnx.load(input_path)
        
        print("🔧 Simplifying model...")
        model_simp, check = onnxsim.simplify(model)
        
        if check:
            print("✅ Model simplified successfully")
            onnx.save(model_simp, output_path)
            print(f"💾 Saved simplified model to: {output_path}")
            return True
        else:
            print("❌ Model simplification failed")
            return False
            
    except Exception as e:
        print(f"❌ Error during simplification: {e}")
        return False

def convert_yolov10_to_yolov8_format():
    """
    Alternative: Convert YOLOv10 to YOLOv8 compatible format
    This is a placeholder for more complex conversion logic
    """
    print("🔄 YOLOv10 to YOLOv8 format conversion")
    print("⚠️  This requires manual model architecture changes")
    print("📝 Recommended: Use YOLOv8n-face.onnx instead")

def main():
    parser = argparse.ArgumentParser(description="Convert YOLOv10 face model for OpenCV compatibility")
    parser.add_argument("--input", "-i", required=True, help="Input YOLOv10 ONNX model path")
    parser.add_argument("--output", "-o", help="Output simplified ONNX model path")
    parser.add_argument("--simplify-only", action="store_true", help="Only simplify the model")
    
    args = parser.parse_args()
    
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"❌ Input model not found: {input_path}")
        return 1
    
    if args.output:
        output_path = Path(args.output)
    else:
        output_path = input_path.parent / f"{input_path.stem}_simplified.onnx"
    
    print("🚀 YOLOv10 Face Model Conversion Tool")
    print("="*50)
    
    if not check_dependencies():
        return 1
    
    if args.simplify_only:
        success = simplify_model(input_path, output_path)
        if success:
            print("\n✅ Model simplification completed!")
            print(f"📁 Output: {output_path}")
            print("\n🔧 Try using the simplified model in your config:")
            print(f"face_model_path={output_path}")
        else:
            print("\n❌ Simplification failed")
            return 1
    else:
        print("\n🎯 Recommendation: Use YOLOv8n-face.onnx instead")
        print("YOLOv8 models have better OpenCV DNN compatibility")
        print("\n📥 Download YOLOv8n-face.onnx from:")
        print("https://github.com/akanametov/yolo-face")
        
        # Still try simplification
        print("\n🔧 Attempting simplification as fallback...")
        success = simplify_model(input_path, output_path)
        if success:
            print("⚠️  Simplified model may still have compatibility issues")
    
    return 0

if __name__ == "__main__":
    sys.exit(main()) 