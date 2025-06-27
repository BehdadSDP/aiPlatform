#include "include/control_unit.h"
#include "include/frame_buffer_manager.h"
#include "include/model_manager.h"
#include "include/safety_manager.h"
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

// Tracking management methods (moved from TrackerManager)
void ControlUnit::initializeTracker(std::unique_ptr<TrackerInterface> tracker, bool showTrackingPath) {
    tracker_ = std::move(tracker);
    showTrackingPath_ = showTrackingPath;
    isTracking_ = false;
    trackedClassId_ = -1;
    trackingPath_.clear();
}

void ControlUnit::runTrackingLoop(std::atomic<bool>& running, ModelManager& modelManager, SafetyManager& safetyManager) {
    int mode = getDetectionMode();

    while (running) {
        if (!waitForTrackingTurn()) {
            continue;
        }

        bool shouldInitialize = hasNewDetection();
        cv::Rect yoloBox;
        cv::Mat detectionFrame;
        int classId = -1;

        if (shouldInitialize) {
            getDetectionData(yoloBox, detectionFrame, classId);
            markDetectionAsProcessed();
        }

        if ((shouldInitialize && mode == 0) || (mode == 1 && !isTracking_)) {
            if (!detectionFrame.empty()) {
                try {
                    if (tracker_ && tracker_->init(detectionFrame, yoloBox)) {
                        isTracking_ = true;
                        lastTrackBox_ = yoloBox;
                        trackedClassId_ = classId;
                        trackingPath_.clear();
                        trackingPath_.push_back(cv::Point(yoloBox.x + yoloBox.width / 2, yoloBox.y + yoloBox.height / 2));
                        
                        std::string className = (classId >= 0 && classId < static_cast<int>(modelManager.getClassNames().size())) ?
                                              modelManager.getClassNames()[classId] : "Unknown";
                        std::cout << "Tracking initialized: " << className << " [" << yoloBox.width << "x" << yoloBox.height << "]" << std::endl;
                    } else {
                        std::cerr << "Tracker initialization failed" << std::endl;
                        isTracking_ = false;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Tracker initialization failed: " << e.what() << std::endl;
                    isTracking_ = false;
                }
            }
        }

        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) continue;

        if (isTracking_) {
            updateTracker(frame);
            
            // Process safety monitoring with tracking data
            if (trackedClassId_ >= 0) {
                std::vector<model::Detection> trackingDetections;
                model::Detection trackingDetection;
                trackingDetection.box = lastTrackBox_;
                trackingDetection.classId = trackedClassId_;
                trackingDetection.confidence = tracker_->getLastConfidence();
                trackingDetections.push_back(trackingDetection);
                
                safetyManager.processDetections(trackingDetections, modelManager.getClassNames());
            }
        }
    }
}

void ControlUnit::updateTracker(const cv::Mat& frame) {
    try {
        if (tracker_) {
            lastTrackBox_ = tracker_->update(frame);
            
            bool trackerValid = lastTrackBox_.width > 0 && lastTrackBox_.height > 0 && tracker_->isInitialized();
            if (!trackerValid) {
                isTracking_ = false;
                trackingPath_.clear();
                trackedClassId_ = -1;  // Reset class ID
                setTrackerFailed(true);
                std::cout << "Tracking lost (confidence: " << tracker_->getLastConfidence() << ")" << std::endl;
            } else {
                trackingPath_.push_back(cv::Point(lastTrackBox_.x + lastTrackBox_.width / 2, lastTrackBox_.y + lastTrackBox_.height / 2));
                if (trackingPath_.size() > MAX_PATH_POINTS) {
                    trackingPath_.erase(trackingPath_.begin());
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

