#include "include/tracking/csrt_tracker.h"
#include "include/logger.h"
#include <iostream>

CSRTTracker::CSRTTracker()
{
    // Create CSRT tracker with default parameters
    try {
        tracker_ = cv::TrackerCSRT::create();
        LOG_INFO("CSRTTracker created successfully");
    } catch (const cv::Exception& e) {
        LOG_ERROR("Failed to create CSRTTracker: {}", e.what());
        throw std::runtime_error("Failed to create CSRTTracker: " + std::string(e.what()));
    }
}

bool CSRTTracker::init(const cv::Mat& frame, const cv::Rect& initBox)
{
    if (frame.empty() || initBox.width <= 0 || initBox.height <= 0) {
        LOG_ERROR("CSRTTracker::init - Invalid frame or initial bounding box");
        return false;
    }

    // Ensure the bounding box is within frame bounds
    trackedBox_ = initBox & cv::Rect(0, 0, frame.cols, frame.rows);
    if (trackedBox_.width <= 0 || trackedBox_.height <= 0) {
        LOG_ERROR("CSRTTracker::init - Initial bounding box is outside frame bounds");
        return false;
    }

    try {
        // Initialize the CSRT tracker
        tracker_->init(frame, trackedBox_);
        LOG_INFO("CSRTTracker initialized with box: [{}, {}, {}, {}]", 
                 trackedBox_.x, trackedBox_.y, trackedBox_.width, trackedBox_.height);
        
        initialized_ = true;
        confidence_ = 1.0f;
        consecutiveFailures_ = 0;
        return true;
        
    } catch (const cv::Exception& e) {
        LOG_ERROR("CSRTTracker::init failed: {}", e.what());
        initialized_ = false;
        confidence_ = 0.0f;
        return false;
    }
}

cv::Rect CSRTTracker::update(const cv::Mat& frame)
{
    if (!initialized_ || frame.empty()) {
        LOG_WARN("CSRTTracker::update - Tracker not initialized or frame is empty");
        return cv::Rect();
    }

    try {
        bool trackingSuccess = tracker_->update(frame, trackedBox_);
        
        // Validate tracking results
        if (!trackingSuccess || trackedBox_.width <= 0 || trackedBox_.height <= 0 ||
            trackedBox_.x < 0 || trackedBox_.y < 0 ||
            trackedBox_.x + trackedBox_.width > frame.cols ||
            trackedBox_.y + trackedBox_.height > frame.rows) {
            
            consecutiveFailures_++;
            LOG_WARN("CSRTTracker::update - Tracking failed (failure count: {})", consecutiveFailures_);
            
            // Declare tracking lost after MAX_FAILURES consecutive failures
            if (consecutiveFailures_ >= MAX_FAILURES) {
                LOG_ERROR("CSRTTracker::update - Tracking lost after {} consecutive failures", MAX_FAILURES);
                initialized_ = false;
                confidence_ = 0.0f;
                return cv::Rect();
            }
            
            // Reduce confidence but keep trying
            confidence_ = std::max(0.0f, confidence_ - 0.2f);
            return trackedBox_; // Return last known good box
        }
        
        // Tracking successful - reset failure counter and restore confidence
        consecutiveFailures_ = 0;
        confidence_ = std::min(1.0f, confidence_ + 0.1f);
        
        // Clamp the bounding box to frame bounds
        trackedBox_.x = std::max(0, trackedBox_.x);
        trackedBox_.y = std::max(0, trackedBox_.y);
        trackedBox_.width = std::min(trackedBox_.width, frame.cols - trackedBox_.x);
        trackedBox_.height = std::min(trackedBox_.height, frame.rows - trackedBox_.y);
        
        return trackedBox_;
        
    } catch (const cv::Exception& e) {
        LOG_ERROR("CSRTTracker::update exception: {}", e.what());
        consecutiveFailures_++;
        
        if (consecutiveFailures_ >= MAX_FAILURES) {
            initialized_ = false;
            confidence_ = 0.0f;
            return cv::Rect();
        }
        
        confidence_ = std::max(0.0f, confidence_ - 0.2f);
        return trackedBox_;
    }
}
