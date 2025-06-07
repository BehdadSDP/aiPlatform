#include "include/control_unit.h"

void ControlUnit::setDetectionMode(int mode) {
    if (mode == 0 || mode == 1) {
        detectionMode_ = mode;
    } else {
        detectionMode_ = 0;
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
        bool shouldRun;
        {
            std::lock_guard<std::mutex> detLock(detectionMutex_);
            shouldRun = !detection_.valid || trackerFailed_;
        }
        
        if (!shouldRun) {
            return detectionCV_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                [this]() {
                    std::lock_guard<std::mutex> detLock(detectionMutex_);
                    return !detection_.valid || trackerFailed_;
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

void ControlUnit::setDetection(const cv::Rect& box, const cv::Mat& frame, uint64_t frameSeq, int classId) {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    detection_.box = box;
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
        frame = detection_.frame.clone();
        classId = detection_.classId;
    }
}

void ControlUnit::markDetectionAsProcessed() {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    detection_.newDetection = false;
}

