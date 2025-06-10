#include "include/siamfc_tracker_adapter.h"
#include <iostream>
#include <stdexcept>

SiamFCPPAdapter::SiamFCPPAdapter(const std::string& featureModelPath, const std::string& trackModelPath) {
    tracker_ = std::make_unique<SiamFCPPTracker2>();

    if (!tracker_->loadModel(featureModelPath, trackModelPath)) {
        throw std::runtime_error("Failed to load SiamFCPP tracker models");
    }
}

void SiamFCPPAdapter::init(const cv::Mat& frame, const cv::Rect& initBox) {
    if (!tracker_->init(frame, initBox)) {
        throw std::runtime_error("Failed to initialize SiamFCPP tracker");
    }
    initialized_ = true;
}

cv::Rect SiamFCPPAdapter::update(const cv::Mat& frame) {
    if (!initialized_) {
        throw std::runtime_error("Tracker not initialized");
    }

    float confidence = 0.0f;
    cv::Rect result = tracker_->update(frame, confidence);
    lastConfidence_ = confidence;

    if (confidence < 0.25f ||
        result.width <= 0 || result.height <= 0 ||
        result.x < 0 || result.y < 0 ||
        result.x + result.width >= frame.cols ||
        result.y + result.height >= frame.rows) {

        failureCount_++;
        
        if (failureCount_ >= 3) {
            initialized_ = false;
            return cv::Rect(0, 0, 0, 0);
        } else {
            return lastValidResult_;
        }
    }
    
    failureCount_ = 0;
    lastValidResult_ = result;
    return result;
}

bool SiamFCPPAdapter::isInitialized() const {
    return initialized_;
}

void SiamFCPPAdapter::model_initializer(const cv::Mat& frame, const cv::Rect& bbox) {
    init(frame, bbox);
}

float SiamFCPPAdapter::getLastConfidence() const {
    return lastConfidence_;
} 