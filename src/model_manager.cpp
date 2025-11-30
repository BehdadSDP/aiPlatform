#include "include/model_manager.h"
#include "include/detection/yolo_detector_impl.h"
#include "include/detection/color_detector.h"
#include "include/detection/apriltag_detector.h"
#include "include/logger.h"
#include <iostream>
#include <fstream>
#include <sstream>

ModelManager::ModelManager() : initialized_(false) {
}

ModelManager::~ModelManager() {
    // Cleanup handled by smart pointers
}

std::unique_ptr<DetectorInterface> ModelManager::createDetector(const ModelConfig& config) {
    try {
        switch (config.type) {
            case ModelType::COCO_GENERAL:
            case ModelType::HELMET_DETECTION:
            case ModelType::FACE_DETECTION: {
                // Create YOLO detector for ONNX models
                LOG_INFO("Creating YOLO detector...");
                return std::make_unique<YOLODetector>(
                    config.modelPath,
                    config.classNamesPath,
                    config.targetClassId
                );
            }
            
            case ModelType::COLOR_DETECTION: {
                // Create Color detector
                LOG_INFO("Creating Color detector...");
                ColorDetector::Config colorConfig;
                
                // Parse color ranges from the config
                for (const auto& colorName : config.colorConfig.targetColors) {
                    ColorDetector::ColorRange range;
                    range.name = colorName;
                    
                    // Define HSV ranges for common colors
                    if (colorName == "red") {
                        // Red wraps around in HSV, so we need two ranges
                        range.lowerBound = cv::Scalar(0, 100, 100);
                        range.upperBound = cv::Scalar(10, 255, 255);
                        colorConfig.colorRanges.push_back(range);
                        
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
                    throw std::runtime_error("No valid color ranges configured");
                }
                
                // Set area constraints
                colorConfig.minArea = static_cast<int>(config.colorConfig.minContourArea);
                colorConfig.maxArea = static_cast<int>(config.colorConfig.maxContourArea);
                
                return std::make_unique<ColorDetector>(colorConfig);
            }
            
            case ModelType::APRILTAG_DETECTION: {
                // Create AprilTag detector
                LOG_INFO("Creating AprilTag detector...");
                AprilTagDetector::Config apriltagConfig;
                
                // Map tag family integer to enum
                switch (config.apriltagConfig.tagFamily) {
                    case 0: apriltagConfig.family = AprilTagDetector::TagFamily::TAG_16h5; break;
                    case 1: apriltagConfig.family = AprilTagDetector::TagFamily::TAG_25h9; break;
                    case 2: 
                    default: apriltagConfig.family = AprilTagDetector::TagFamily::TAG_36h11; break;
                }
                
                // Set target tag IDs (empty = detect all)
                apriltagConfig.targetTagIds = config.apriltagConfig.targetTagIds;
                
                // Set detection parameters
                apriltagConfig.minMarkerPerimeter = config.apriltagConfig.minMarkerPerimeter;
                apriltagConfig.maxMarkerPerimeter = config.apriltagConfig.maxMarkerPerimeter;
                apriltagConfig.refineDetection = config.apriltagConfig.refineDetection;
                
                return std::make_unique<AprilTagDetector>(apriltagConfig);
            }
            
            default:
                throw std::runtime_error("Unknown detector type");
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create detector: {}", e.what());
        return nullptr;
    }
}

bool ModelManager::initialize(const ModelConfig& config) {
    try {
        currentConfig_ = config;
        
        // Determine target class ID based on model type for YOLO models
        if (config.type == ModelType::HELMET_DETECTION) {
            // Load class names temporarily to determine helmet class ID
            std::ifstream file(config.classNamesPath);
            if (file.is_open()) {
                std::vector<std::string> tempClassNames;
                std::string line;
                while (std::getline(file, line)) {
                    line.erase(0, line.find_first_not_of(" \t\r\n"));
                    line.erase(line.find_last_not_of(" \t\r\n") + 1);
                    if (!line.empty()) {
                        tempClassNames.push_back(line);
                    }
                }
                file.close();
                
                // Find helmet class
                for (int i = 0; i < static_cast<int>(tempClassNames.size()); ++i) {
                    if (tempClassNames[i] == "helmet" || tempClassNames[i] == "hardhat") {
                        currentConfig_.targetClassId = i;
                        break;
                    }
                }
            }
        } else if (config.type == ModelType::FACE_DETECTION) {
            currentConfig_.targetClassId = 0;
        }
        
        // Use factory to create the detector
        detector_ = createDetector(currentConfig_);
        
        if (!detector_) {
            LOG_ERROR("Failed to create detector");
            initialized_ = false;
            return false;
        }
        
        if (!detector_->isInitialized()) {
            LOG_ERROR("Detector initialization failed");
            initialized_ = false;
            return false;
        }
        
        // Update class names from detector
        currentConfig_.classNames = detector_->getClassNames();
        
        initialized_ = true;
        LOG_INFO("ModelManager initialized successfully:");
        LOG_INFO("  Detector: {}", detector_->getName());
        LOG_INFO("  Classes loaded: {}", currentConfig_.classNames.size());
        LOG_INFO("  Target class ID: {}", currentConfig_.targetClassId);
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize ModelManager: {}", e.what());
        initialized_ = false;
        return false;
    }
}

std::vector<model::Detection> ModelManager::detect(const cv::Mat& frame) {
    if (!initialized_ || !detector_) {
        LOG_ERROR("ModelManager not initialized");
        return {};
    }
    
    // Polymorphic call - works for all detector types!
    return detector_->detect(frame);
}

const std::vector<std::string>& ModelManager::getClassNames() const {
    return currentConfig_.classNames;
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
        case ModelType::APRILTAG_DETECTION:
            return "AprilTag Detection";
        default:
            return "Unknown Model";
    }
}
