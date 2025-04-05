#include "vittracker.h"
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
    } catch (const cv::Exception& e) {
        throw std::runtime_error("Failed to create TrackerVit: " + std::string(e.what()));
    }
}

void VitTracker::init(const cv::Mat& frame, const cv::Rect& initBox)
{
    if (frame.empty() || initBox.width <= 0 || initBox.height <= 0) {
        throw std::runtime_error("Invalid frame or initial bounding box in VitTracker::init");
    }

    // Ensure the bounding box is within frame bounds
    trackedBox_ = initBox & cv::Rect(0, 0, frame.cols, frame.rows);
    if (trackedBox_.width <= 0 || trackedBox_.height <= 0) {
        throw std::runtime_error("Initial bounding box is outside frame bounds");
    }

    // Initialize the tracker
    try {
        tracker_->init(frame, trackedBox_);
    } catch (const cv::Exception& e) {
        throw std::runtime_error("TrackerVit::init failed: " + std::string(e.what()));
    }

    initialized_ = true;
    trackScore_ = 1.0f; // Reset confidence
}

cv::Rect VitTracker::update(const cv::Mat& frame)
{
    if (!initialized_ || frame.empty()) {
        return cv::Rect(); // Return empty rect if not initialized or frame is invalid
    }

    // Update the tracker
    bool isLocated = false;
    try {
        isLocated = tracker_->update(frame, trackedBox_);
        trackScore_ = tracker_->getTrackingScore();
    } catch (const cv::Exception& e) {
        std::cerr << "TrackerVit::update failed: " << e.what() << std::endl;
        isLocated = false;
        trackScore_ = 0.0f;
    }

    // If tracking fails or score is too low, mark as uninitialized
    if (!isLocated || trackScore_ < 0.3f || trackedBox_.width <= 0 || trackedBox_.height <= 0) {
        initialized_ = false;
        trackScore_ = 0.0f;
        return cv::Rect(); // Return empty rect to indicate tracking loss
    }

    return trackedBox_;
}
