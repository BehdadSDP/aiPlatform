#include "include/control_unit.h"
#include "include/frame_buffer_manager.h"
#include "include/model_manager.h"
#include <iostream>
#include <chrono>

void ControlUnit::setDetectionMode(int mode) {
    if (mode == 0 || mode == 1) {
        detectionMode_ = mode;
    } else {
        detectionMode_ = 0;
    }
}

void ControlUnit::setOperationMode(int mode) {
    if (mode == 0 || mode == 1) {
        operationMode_ = mode;
    } else {
        operationMode_ = 0;
    }
}

void ControlUnit::setDetectionInterval(int milliseconds) {
    if (milliseconds > 0) {
        std::lock_guard<std::mutex> lock(syncMutex_);
        detectionInterval_ = milliseconds;
    }
}

void ControlUnit::setTrackingInterval(int milliseconds) {
    if (milliseconds > 0) {
        std::lock_guard<std::mutex> lock(syncMutex_);
        trackingInterval_ = milliseconds;
    }
}

void ControlUnit::notifyNewFrame() {
    detectionCV_.notify_one();
    trackingCV_.notify_one();
}

bool ControlUnit::waitForDetectionTurn(int timeoutMs) {
    std::unique_lock<std::mutex> lock(syncMutex_);
    
    if (detectionMode_ == 0) {
        // Mode 0: Time-based detection (interval-based)
        auto now = std::chrono::steady_clock::now();
        if (lastDetectionTime_ + std::chrono::milliseconds(detectionInterval_) > now) {
            return detectionCV_.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
                [this, now]() { 
                    return lastDetectionTime_ + std::chrono::milliseconds(detectionInterval_) <= now; 
                });
        }
        
        lastDetectionTime_ = now;
        return true;
    }
    else {
        // Mode 1: Tracker-initialization-based detection (continuous)
        // Detection should run when:
        // 1. No tracker is currently running (!isTracking_) OR
        // 2. Tracker has failed (trackerFailed_)
        bool shouldRun;
        {
            std::lock_guard<std::mutex> detLock(detectionMutex_);
            shouldRun = !isTracking_ || trackerFailed_;
        }
        
        if (!shouldRun) {
            // Wait until tracker fails or stops tracking
            return detectionCV_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                [this]() {
                    std::lock_guard<std::mutex> detLock(detectionMutex_);
                    return !isTracking_ || trackerFailed_;
                });
        }
        
        return shouldRun;
    }
}

bool ControlUnit::waitForTrackingTurn(int timeoutMs) {
    std::unique_lock<std::mutex> lock(syncMutex_);
    
    auto now = std::chrono::steady_clock::now();
    if (lastTrackingTime_ + std::chrono::milliseconds(trackingInterval_) > now) {
        return trackingCV_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [this, now]() {
                return lastTrackingTime_ + std::chrono::milliseconds(trackingInterval_) <= now;
            });
    }
    
    lastTrackingTime_ = now;
    return true;
}

bool ControlUnit::hasValidDetection() const {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    return detection_.valid;
}

bool ControlUnit::hasNewDetection() const {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    return detection_.newDetection && detection_.valid;
}

bool ControlUnit::hasTrackerFailed() const {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    return trackerFailed_;
}

void ControlUnit::setTrackerFailed(bool failed) {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    trackerFailed_ = failed;
    if (failed) {
        detectionCV_.notify_one();
    }
}

void ControlUnit::setTrackerFailed(bool failed, const cv::Mat& frame, const cv::Rect& box) {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    trackerFailed_ = failed;
    if (failed) {
        lastFailedFrame_ = frame.clone();
        lastFailedBox_ = box;
        detectionCV_.notify_one();
    }
}

void ControlUnit::getFailureData(cv::Mat& frame, cv::Rect& box) const {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    frame = lastFailedFrame_.clone();
    box = lastFailedBox_;
}

void ControlUnit::setDetection(const cv::Rect& box, const cv::Mat& frame, uint64_t frameSeq, int classId) {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    detection_.box = box;
    // ✅ OPTIMIZED: Only clone when absolutely necessary for thread safety
    detection_.frame = frame.clone();
    detection_.frameSeq = frameSeq;
    detection_.classId = classId;
    detection_.valid = true;
    detection_.newDetection = true;
    trackerFailed_ = false;
    
    trackingCV_.notify_one();
}

void ControlUnit::clearDetection() {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    detection_.valid = false;
    detection_.newDetection = false;
    detection_.frame.release();
    detection_.classId = -1;
}

void ControlUnit::getDetectionData(cv::Rect& box, cv::Mat& frame, int& classId) const {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    if (detection_.valid) {
        box = detection_.box;
        // ✅ OPTIMIZED: Only clone when absolutely necessary
        frame = detection_.frame.clone();
        classId = detection_.classId;
    }
}

void ControlUnit::markDetectionAsProcessed() {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    detection_.newDetection = false;
}

void ControlUnit::setIsTracking(bool isTracking) {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    isTracking_ = isTracking;
}

bool ControlUnit::isTracking() const {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    return isTracking_;
}

