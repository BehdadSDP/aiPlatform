#include "include/siamfc_tracker_adapter.h"
#include <iostream>
#include <stdexcept>

SiamFCPPAdapter::SiamFCPPAdapter(const std::string& featureModelPath, const std::string& trackModelPath) {
    // Initialize the tracker
    tracker_ = std::make_unique<SiamFCPPTracker2>();

    // Load models
    if (!tracker_->loadModel(featureModelPath, trackModelPath)) {
        throw std::runtime_error("Failed to load SiamFCPP tracker models");
    }
    std::cout << "SiamFCPP tracker models loaded successfully" << std::endl;
}

void SiamFCPPAdapter::init(const cv::Mat& frame, const cv::Rect& initBox) {
    std::cout << "SiamFCPPAdapter::init - bbox: [" << initBox.x << ", " << initBox.y
             << ", " << initBox.width << ", " << initBox.height << "]" << std::endl;

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

    // Store confidence for possible later use
    lastConfidence_ = confidence;

    // Check if tracking is still valid based on confidence and box validity
    if (confidence < 0.25f ||
        result.width <= 0 || result.height <= 0 ||
        result.x < 0 || result.y < 0 ||
        result.x + result.width >= frame.cols ||
        result.y + result.height >= frame.rows) {

        std::cout << "Tracking failed - confidence: " << confidence
                  << ", box: " << result.x << "," << result.y << ","
                  << result.width << "," << result.height << std::endl;

        // Introduce a counter to make tracking failure more robust
        failureCount_++;
        
        // Only declare tracking lost after multiple consecutive failures
        if (failureCount_ >= 3) {
            std::cout << "Too many consecutive failures, tracking lost" << std::endl;
            initialized_ = false;
            return cv::Rect(0, 0, 0, 0);  // Return empty rect to indicate failure
        } else {
            // Return the last valid result for a few frames to handle temporary low confidence
            return lastValidResult_;
        }
    }
    
    // Reset failure counter and store valid result
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