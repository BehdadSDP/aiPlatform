#include "include/model_manager.h"
#include "include/detection/color_detector.h"
#include "include/logger.h"
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
                LOG_ERROR("Failed to load class names from: {}", config.classNamesPath);
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
            
            LOG_INFO("ModelManager initialized successfully:");
            std::string modelTypeName = (config.type == ModelType::HELMET_DETECTION) ? "Helmet Detection" :
                                       (config.type == ModelType::FACE_DETECTION) ? "Face Detection (YOLOv10n)" :
                                       "COCO General";
            LOG_INFO("  Model type: {}", modelTypeName);
            LOG_INFO("  Model path: {}", config.modelPath);
            LOG_INFO("  Classes loaded: {}", currentConfig_.classNames.size());
            LOG_INFO("  Target class ID: {}", currentConfig_.targetClassId);
        }
        
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize ModelManager: {}", e.what());
        initialized_ = false;
        return false;
    }
}

std::vector<model::Detection> ModelManager::detect(const cv::Mat& frame) {
    if (!initialized_) {
        LOG_ERROR("ModelManager not initialized");
        return {};
    }
    
    // Use color detector if in color detection mode
    if (currentConfig_.type == ModelType::COLOR_DETECTION && colorDetector_) {
        return colorDetector_->detect(frame);
    }
    
    if (!activeModel_) {
        LOG_ERROR("ONNX model not loaded");
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
        LOG_ERROR("Cannot open class names file: {}", classNamesPath);
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
        LOG_ERROR("No class names loaded from: {}", classNamesPath);
        return false;
    }
    
    LOG_INFO("Loaded {} class names from: {}", classNames.size(), classNamesPath);
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
            LOG_WARN("Unknown color '{}', skipping", colorName);
        }
    }
    
    if (colorConfig.colorRanges.empty()) {
        LOG_ERROR("No valid color ranges configured");
        return false;
    }
    
    // Set area constraints
    colorConfig.minArea = static_cast<int>(config.colorConfig.minContourArea);
    colorConfig.maxArea = static_cast<int>(config.colorConfig.maxContourArea);
    
    // Create the color detector
    colorDetector_ = std::make_unique<ColorDetector>(colorConfig);
    
    LOG_INFO("ModelManager initialized successfully:");
    LOG_INFO("  Model type: Color Detection");
    std::string colors;
    for (const auto& colorName : config.colorConfig.targetColors) {
        colors += colorName + " ";
    }
    LOG_INFO("  Target colors: {}", colors);
    LOG_INFO("  Min area: {}", colorConfig.minArea);
    LOG_INFO("  Max area: {}", colorConfig.maxArea);
    
    initialized_ = true;
    return true;
}

std::string ModelManager::getModelName() const {
    switch (currentConfig_.type) {
        case ModelType::COCO_GENERAL:
            return "COCO General Detection";
        case ModelType::HELMET_DETECTION:
            return "Helmet Detection";
        case ModelType::FACE_DETECTION:
            return "Face Detection";
        case ModelType::COLOR_DETECTION:
            return "Color Detection";
        default:
            return "Unknown Model";
    }
}
