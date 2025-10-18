#include "include/model_manager.h"
#include "include/detection/color_detector.h"
#include <iostream>
#include <fstream>

ModelManager::ModelManager() : initialized_(false) {
}

ModelManager::~ModelManager() {
    // Cleanup handled by smart pointers
}

bool ModelManager::initialize(const ModelConfig& config) {
    try {
        currentConfig_ = config;
        
        // Handle color detection separately
        if (config.type == ModelType::COLOR_DETECTION) {
            return initializeColorDetection(config);
        }
        
        {
            // Load class names for ONNX models
            if (!loadClassNames(config.classNamesPath, currentConfig_.classNames)) {
                std::cerr << "Failed to load class names from: " << config.classNamesPath << std::endl;
                return false;
            }
            
            // Determine target class ID based on model type
            if (config.type == ModelType::HELMET_DETECTION) {
                currentConfig_.targetClassId = getHelmetTargetClassId();
            } else if (config.type == ModelType::FACE_DETECTION) {
                // For face detection, we want to detect faces (typically class 0)
                currentConfig_.targetClassId = 0;
            }
            
            // Initialize the ONNX YOLO model
            activeModel_ = std::make_unique<model>(config.modelPath, config.classNamesPath, currentConfig_.targetClassId);
            
            std::cout << "ModelManager initialized successfully:" << std::endl;
            std::string modelTypeName = (config.type == ModelType::HELMET_DETECTION) ? "Helmet Detection" :
                                       (config.type == ModelType::FACE_DETECTION) ? "Face Detection (YOLOv10n)" :
                                       "COCO General";
            std::cout << "  Model type: " << modelTypeName << std::endl;
            std::cout << "  Model path: " << config.modelPath << std::endl;
            std::cout << "  Classes loaded: " << currentConfig_.classNames.size() << std::endl;
            std::cout << "  Target class ID: " << currentConfig_.targetClassId << std::endl;
        }
        
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize ModelManager: " << e.what() << std::endl;
        initialized_ = false;
        return false;
    }
}

std::vector<model::Detection> ModelManager::detect(const cv::Mat& frame) {
    if (!initialized_) {
        std::cerr << "ModelManager not initialized" << std::endl;
        return {};
    }
    
    // Use color detector if in color detection mode
    if (currentConfig_.type == ModelType::COLOR_DETECTION && colorDetector_) {
        return colorDetector_->detect(frame);
    }
    
    if (!activeModel_) {
        std::cerr << "ONNX model not loaded" << std::endl;
        return {};
    }
    
    return activeModel_->detect(frame);
}

std::string ModelManager::getDetectionDescription(const model::Detection& detection) const {
    std::string className = "Unknown";
    
    if (detection.classId >= 0 && detection.classId < static_cast<int>(currentConfig_.classNames.size())) {
        className = currentConfig_.classNames[detection.classId];
    }
    
    std::string description = className + " (" + std::to_string(int(detection.confidence * 100)) + "%)";
    
    // Add specific descriptions for different model types
    if (currentConfig_.type == ModelType::HELMET_DETECTION) {
        if (className == "helmet" || className == "hardhat") {
            description += " - SAFETY COMPLIANT";
        } else if (className == "no-helmet" || className == "head") {
            description += " - SAFETY VIOLATION!";
        } else if (className == "person") {
            description += " - Person detected";
        }
    }
    else if (currentConfig_.type == ModelType::FACE_DETECTION) {
        if (className == "face") {
            description += " - Face detected";
        }
    }
    
    return description;
}

bool ModelManager::loadClassNames(const std::string& classNamesPath, std::vector<std::string>& classNames) {
    std::ifstream file(classNamesPath);
    if (!file.is_open()) {
        std::cerr << "Cannot open class names file: " << classNamesPath << std::endl;
        return false;
    }
    
    classNames.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            classNames.push_back(line);
        }
    }
    
    if (classNames.empty()) {
        std::cerr << "No class names loaded from: " << classNamesPath << std::endl;
        return false;
    }
    
    std::cout << "Loaded " << classNames.size() << " class names from: " << classNamesPath << std::endl;
    return true;
}

int ModelManager::getHelmetTargetClassId() const {
    // For helmet detection, we want to detect both helmets and no-helmets
    // Let's use helmet (class 0) as the primary target, but we'll process all detections
    for (int i = 0; i < static_cast<int>(currentConfig_.classNames.size()); ++i) {
        const std::string& className = currentConfig_.classNames[i];
        if (className == "helmet" || className == "hardhat") {
            return i;
        }
    }
    
    // If no helmet class found, return 0 (first class)
    return 0;
}

bool ModelManager::initializeColorDetection(const ModelConfig& config) {
    ColorDetector::Config colorConfig;
    
    // Parse color ranges from the config
    for (const auto& colorName : config.colorConfig.targetColors) {
        ColorDetector::ColorRange range;
        range.name = colorName;
        
        // Define HSV ranges for common colors
        if (colorName == "red") {
            // Red wraps around in HSV, so we need two ranges
            // Lower red range: 0-10
            range.lowerBound = cv::Scalar(0, 100, 100);
            range.upperBound = cv::Scalar(10, 255, 255);
            colorConfig.colorRanges.push_back(range);
            
            // Upper red range: 170-180
            range.lowerBound = cv::Scalar(170, 100, 100);
            range.upperBound = cv::Scalar(180, 255, 255);
            colorConfig.colorRanges.push_back(range);
        }
        else if (colorName == "blue") {
            range.lowerBound = cv::Scalar(100, 100, 100);
            range.upperBound = cv::Scalar(130, 255, 255);
            colorConfig.colorRanges.push_back(range);
        }
        else if (colorName == "green") {
            range.lowerBound = cv::Scalar(40, 50, 50);
            range.upperBound = cv::Scalar(80, 255, 255);
            colorConfig.colorRanges.push_back(range);
        }
        else if (colorName == "yellow") {
            range.lowerBound = cv::Scalar(20, 100, 100);
            range.upperBound = cv::Scalar(35, 255, 255);
            colorConfig.colorRanges.push_back(range);
        }
        else if (colorName == "orange") {
            range.lowerBound = cv::Scalar(10, 100, 100);
            range.upperBound = cv::Scalar(20, 255, 255);
            colorConfig.colorRanges.push_back(range);
        }
        else if (colorName == "purple") {
            range.lowerBound = cv::Scalar(130, 50, 50);
            range.upperBound = cv::Scalar(160, 255, 255);
            colorConfig.colorRanges.push_back(range);
        }
        else {
            std::cerr << "Warning: Unknown color '" << colorName << "', skipping" << std::endl;
        }
    }
    
    if (colorConfig.colorRanges.empty()) {
        std::cerr << "Error: No valid color ranges configured" << std::endl;
        return false;
    }
    
    // Set area constraints
    colorConfig.minArea = static_cast<int>(config.colorConfig.minContourArea);
    colorConfig.maxArea = static_cast<int>(config.colorConfig.maxContourArea);
    
    // Create the color detector
    colorDetector_ = std::make_unique<ColorDetector>(colorConfig);
    
    std::cout << "ModelManager initialized successfully:" << std::endl;
    std::cout << "  Model type: Color Detection" << std::endl;
    std::cout << "  Target colors: ";
    for (const auto& colorName : config.colorConfig.targetColors) {
        std::cout << colorName << " ";
    }
    std::cout << std::endl;
    std::cout << "  Min area: " << colorConfig.minArea << std::endl;
    std::cout << "  Max area: " << colorConfig.maxArea << std::endl;
    
    initialized_ = true;
    return true;
}
