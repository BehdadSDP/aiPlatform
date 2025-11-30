#include "include/detection/yolo_detector_impl.h"
#include "include/logger.h"
#include <fstream>

YOLODetector::YOLODetector(const std::string& modelPath, 
                           const std::string& namesPath, 
                           int targetClassId)
    : initialized_(false)
{
    try {
        // Load class names first
        if (!loadClassNames(namesPath)) {
            LOG_ERROR("Failed to load class names from: {}", namesPath);
            return;
        }
        
        // Determine model type from path for better logging
        if (modelPath.find("helmet") != std::string::npos) {
            modelType_ = "YOLO-Helmet";
        } else if (modelPath.find("face") != std::string::npos) {
            modelType_ = "YOLO-Face";
        } else if (modelPath.find("vehicle") != std::string::npos) {
            modelType_ = "YOLO-Vehicle";
        } else {
            modelType_ = "YOLO-COCO";
        }
        
        // Create the underlying YOLO model
        yoloModel_ = std::make_unique<model>(modelPath, namesPath, targetClassId);
        
        initialized_ = true;
        LOG_INFO("YOLODetector initialized successfully");
        LOG_INFO("  Type: {}", modelType_);
        LOG_INFO("  Model: {}", modelPath);
        LOG_INFO("  Classes: {}", classNames_.size());
        LOG_INFO("  Target class ID: {}", targetClassId);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize YOLODetector: {}", e.what());
        initialized_ = false;
    }
}

std::vector<model::Detection> YOLODetector::detect(const cv::Mat& frame) {
    if (!initialized_ || !yoloModel_) {
        LOG_ERROR("YOLODetector not initialized");
        return {};
    }
    
    return yoloModel_->detect(frame);
}

std::string YOLODetector::getName() const {
    return modelType_;
}

bool YOLODetector::isInitialized() const {
    return initialized_;
}

std::vector<std::string> YOLODetector::getClassNames() const {
    return classNames_;
}

bool YOLODetector::loadClassNames(const std::string& namesPath) {
    std::ifstream file(namesPath);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open class names file: {}", namesPath);
        return false;
    }
    
    classNames_.clear();
    std::string line;
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) {
            classNames_.push_back(line);
        }
    }
    file.close();
    
    if (classNames_.empty()) {
        LOG_ERROR("No class names loaded from: {}", namesPath);
        return false;
    }
    
    LOG_DEBUG("Loaded {} class names from: {}", classNames_.size(), namesPath);
    return true;
}
