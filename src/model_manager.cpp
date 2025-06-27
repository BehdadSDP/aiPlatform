#include "include/model_manager.h"
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
    
    // Add specific descriptions for helmet detection
    if (currentConfig_.type == ModelType::HELMET_DETECTION) {
        if (className == "helmet" || className == "hardhat") {
            description += " - SAFETY COMPLIANT";
        } else if (className == "no-helmet" || className == "head") {
            description += " - SAFETY VIOLATION!";
        } else if (className == "person") {
            description += " - Person detected";
        }
    }
    // Add specific descriptions for face detection
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