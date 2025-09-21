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

// Tracking management methods (moved from TrackerManager)
void ControlUnit::initializeTracker(std::unique_ptr<TrackerInterface> tracker, bool showTrackingPath) {
    tracker_ = std::move(tracker);
    showTrackingPath_ = showTrackingPath;
    isTracking_ = false;
    trackedClassId_ = -1;
    trackingPath_.clear();
}

bool ControlUnit::startTracking(const cv::Mat& frame, const cv::Rect& box, int classId, const std::vector<std::string>& classNames) {
    try {
        if (tracker_ && tracker_->init(frame, box)) {
            isTracking_ = true;
            lastTrackBox_ = box;
            trackedClassId_ = classId;
            trackingPath_.clear();
            
            // Only build tracking path if visualization is enabled
            if (showTrackingPath_) {
                trackingPath_.push_back(cv::Point(box.x + box.width / 2, box.y + box.height / 2));
            }
            
            // Clear tracker failure flag and detection data since we successfully started tracking
            {
                std::lock_guard<std::mutex> lock(detectionMutex_);
                trackerFailed_ = false;
                // Clear detection to prevent repeated initialization attempts
                detection_.valid = false;
                detection_.newDetection = false;
            }
            
            std::string className = (classId >= 0 && classId < static_cast<int>(classNames.size())) ?
                                  classNames[classId] : "Unknown";
            std::cout << "Tracking initialized: " << className << " [" << box.width << "x" << box.height << "]" << std::endl;
            
            // In detection mode 1, notify detection thread that it should stop running
            if (detectionMode_ == 1) {
                detectionCV_.notify_one();
            }
            
            return true;
        } else {
            std::cerr << "Tracker initialization failed" << std::endl;
            isTracking_ = false;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Tracker initialization failed: " << e.what() << std::endl;
        isTracking_ = false;
        return false;
    }
}

void ControlUnit::runTrackingLoop(std::atomic<bool>& running, ModelManager& modelManager) {
    while (running) {
        if (!waitForTrackingTurn()) {
            continue;
        }

        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) continue;

        if (isTracking_) {
            updateTracker(frame);
        }
    }
}

void ControlUnit::updateTracker(const cv::Mat& frame) {
    try {
        if (tracker_) {
            cv::Rect newTrackBox = tracker_->update(frame);
            
            bool trackerValid = newTrackBox.width > 0 && newTrackBox.height > 0 && tracker_->isInitialized();
            if (!trackerValid) {
                isTracking_ = false;
                trackingPath_.clear();
                trackedClassId_ = -1;  // Reset class ID
                
                // Only set failure reference if we have a valid previous box
                if (lastTrackBox_.width > 0 && lastTrackBox_.height > 0) {
                    setTrackerFailed(true, frame, lastTrackBox_);
                    std::cout << "Tracking lost (confidence: " << tracker_->getLastConfidence() << ") - Re-enabling detection for re-initialization" << std::endl;
                } else {
                    setTrackerFailed(true);
                    std::cout << "Tracking lost (confidence: " << tracker_->getLastConfidence() << ") - Re-enabling detection" << std::endl;
                }
                
                // In detection mode 1, notify detection thread that it should resume
                if (detectionMode_ == 1) {
                    detectionCV_.notify_one();
                }
            } else {
                lastTrackBox_ = newTrackBox;  // Update the last valid box
                
                // Only update tracking path if visualization is enabled
                if (showTrackingPath_) {
                    trackingPath_.push_back(cv::Point(lastTrackBox_.x + lastTrackBox_.width / 2, lastTrackBox_.y + lastTrackBox_.height / 2));
                    if (trackingPath_.size() > MAX_PATH_POINTS) {
                        trackingPath_.erase(trackingPath_.begin());
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Tracker update failed: " << e.what() << std::endl;
        isTracking_ = false;
        trackingPath_.clear();
        trackedClassId_ = -1;  // Reset class ID
        setTrackerFailed(true);
    }
}

