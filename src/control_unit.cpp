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
                initializeTrackerInternal(detectionFrame, yoloBox, classId, modelManager.getClassNames());
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

        visualizeTracking(frame, safetyManager);
    }
    cv::destroyAllWindows();
}

void ControlUnit::initializeTrackerInternal(const cv::Mat& frame, const cv::Rect& bbox, int classId,
                                           const std::vector<std::string>& classNames) {
    try {
        if (tracker_ && tracker_->init(frame, bbox)) {
            isTracking_ = true;
            lastTrackBox_ = bbox;
            trackedClassId_ = classId;  // Store the class ID
            
            std::string className = (classId >= 0 && classId < static_cast<int>(classNames.size())) ?
                                  classNames[classId] : "Unknown";
            std::cout << "Tracking initialized: " << className << " [" << bbox.width << "x" << bbox.height << "]" << std::endl;

            // Create a temporary safety manager for initialization visualization
            SafetyManager tempSafetyManager;
            visualizeTracking(frame, tempSafetyManager);
        } else {
            std::cerr << "Tracker initialization failed" << std::endl;
            isTracking_ = false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Tracker initialization failed: " << e.what() << std::endl;
        isTracking_ = false;
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

void ControlUnit::visualizeTracking(const cv::Mat& frame, SafetyManager& safetyManager) {
    if (frame.empty()) return;

    cv::Mat displayFrame = frame.clone();
    
    // Draw safety overlays (hazard zones and traffic intensity)
    safetyManager.drawSafetyOverlays(displayFrame);
    
    if (isTracking_ && tracker_ && tracker_->isInitialized()) {
        cv::Point currentCenter(lastTrackBox_.x + lastTrackBox_.width / 2, 
                               lastTrackBox_.y + lastTrackBox_.height / 2);
        
        if (showTrackingPath_) {
            trackingPath_.push_back(currentCenter);
            
            if (trackingPath_.size() > MAX_PATH_POINTS) {
                trackingPath_.erase(trackingPath_.begin());
            }
            
            if (trackingPath_.size() > 1) {
                for (size_t i = 1; i < trackingPath_.size(); ++i) {
                    float alpha = static_cast<float>(i) / trackingPath_.size();
                    int thickness = static_cast<int>(1 + alpha * 3);
                    cv::Scalar fadeColor = pathColor_ * alpha;
                    cv::line(displayFrame, trackingPath_[i-1], trackingPath_[i], fadeColor, thickness);
                }
                
                for (size_t i = 0; i < trackingPath_.size(); ++i) {
                    float alpha = static_cast<float>(i) / trackingPath_.size();
                    int radius = static_cast<int>(2 + alpha * 3);
                    cv::Scalar pointColor = pathColor_ * alpha;
                    cv::circle(displayFrame, trackingPath_[i], radius, pointColor, -1);
                }
            }
            
            cv::circle(displayFrame, currentCenter, 6, cv::Scalar(0, 255, 255), 2);
            cv::circle(displayFrame, currentCenter, 3, cv::Scalar(255, 255, 255), -1);
        }
        
        cv::rectangle(displayFrame, lastTrackBox_, cv::Scalar(0, 0, 255), 3);

        std::string label;
        if (showTrackingPath_) {
            label = "Tracking (Path: " + std::to_string(trackingPath_.size()) + " points)";
        } else {
            label = "Tracking (Path: OFF)";
        }
        
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
        cv::rectangle(displayFrame,
                     cv::Point(lastTrackBox_.x, lastTrackBox_.y - textSize.height - 10),
                     cv::Point(lastTrackBox_.x + textSize.width, lastTrackBox_.y),
                     cv::Scalar(0, 0, 255), -1);

        cv::putText(displayFrame, label,
                   cv::Point(lastTrackBox_.x, lastTrackBox_.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
    }

    cv::imshow("Tracking", displayFrame);
    cv::waitKey(1);
}

