#include "include/tracking/vittracker.h"
#include <stdexcept>
#include <iostream>

VitTracker::VitTracker(const std::string& onnxPath)
{
    // Set up tracker parameters
    params_.net = onnxPath;
    params_.backend = cv::dnn::DNN_BACKEND_OPENCV; // Default to OpenCV backend
    params_.target = cv::dnn::DNN_TARGET_CPU;      // Default to CPU target

    // Create the TrackerVit instance
    try {
        tracker_ = cv::TrackerVit::create(params_);
        std::cout << "VitTracker initialized with model: " << onnxPath << std::endl;
    } catch (const cv::Exception& e) {
        throw std::runtime_error("Failed to create TrackerVit: " + std::string(e.what()));
    }
}

bool VitTracker::init(const cv::Mat& frame, const cv::Rect& initBox)
{
    if (frame.empty() || initBox.width <= 0 || initBox.height <= 0) {
        std::cerr << "Invalid frame or initial bounding box in VitTracker::init" << std::endl;
        return false;
    }

    // Ensure the bounding box is within frame bounds
    trackedBox_ = initBox & cv::Rect(0, 0, frame.cols, frame.rows);
    if (trackedBox_.width <= 0 || trackedBox_.height <= 0) {
        std::cerr << "Initial bounding box is outside frame bounds" << std::endl;
        return false;
    }

    // Make sure to use a deep copy of the frame
    cv::Mat frameCopy = frame.clone();

    // Initialize the tracker
    try {
        tracker_->init(frameCopy, trackedBox_);
        std::cout << "VitTracker initialized with box: " << trackedBox_ << std::endl;
        initialized_ = true;
        trackScore_ = 1.0f; // Reset confidence
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "TrackerVit::init failed: " << e.what() << std::endl;
        initialized_ = false;
        trackScore_ = 0.0f;
        return false;
    }
}

cv::Rect VitTracker::update(const cv::Mat& frame)
{
    if (!initialized_ || frame.empty()) {
        return cv::Rect(); // Return empty rect if not initialized or frame is invalid
    }

    // Make a clone of the frame to avoid any potential memory issues
    cv::Mat frameCopy = frame.clone();

    // Update the tracker
    bool isLocated = false;
    try {
        isLocated = tracker_->update(frameCopy, trackedBox_);
        trackScore_ = tracker_->getTrackingScore();
    } catch (const cv::Exception& e) {
        std::cerr << "TrackerVit::update failed: " << e.what() << std::endl;
        isLocated = false;
        trackScore_ = 0.0f;
    }

    // Validate tracking results
    if (!isLocated || trackScore_ < 0.15f) {
        std::cout << "VitTracker: Tracking lost, score: " << trackScore_ << std::endl;
        initialized_ = false;
        trackScore_ = 0.0f;
        return cv::Rect(); // Return empty rect to indicate tracking loss
    }

    // Validate bounding box dimensions and position
    if (trackedBox_.width <= 0 || trackedBox_.height <= 0 || 
        trackedBox_.x < 0 || trackedBox_.y < 0 || 
        trackedBox_.x + trackedBox_.width > frame.cols || 
        trackedBox_.y + trackedBox_.height > frame.rows) {
        
        std::cout << "VitTracker: Invalid bounding box: " << trackedBox_ << std::endl;
        initialized_ = false;
        trackScore_ = 0.0f;
        return cv::Rect(); // Return empty rect to indicate tracking loss
    }

    // Ensure the box is within the frame bounds
    trackedBox_ = trackedBox_ & cv::Rect(0, 0, frame.cols, frame.rows);
    if (trackedBox_.width <= 0 || trackedBox_.height <= 0) {
        std::cout << "VitTracker: Box outside frame after bounds check" << std::endl;
        initialized_ = false;
        trackScore_ = 0.0f;
        return cv::Rect();
    }

    return trackedBox_;
}
