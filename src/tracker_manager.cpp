#include "include/tracker_manager.h"
#include "include/tracking/vittracker.h"
#include "include/tracking/siamfc_pp_tracker.h"
#include "include/logger.h"
#include <iostream>
#include <stdexcept>

TrackerManager::TrackerManager() : isTracking_(false), showTrackingPath_(true), trackedClassId_(-1) {}

std::unique_ptr<TrackerInterface> TrackerManager::createTracker(const TrackerConfig& config) {
    switch (config.type) {
        case TrackerType::VIT_TRACKER:
            return std::make_unique<VitTracker>(config.vitModelPath);
            
        case TrackerType::SIAMFC_TRACKER: {
            auto tracker = std::make_unique<SiamFCPPTracker2>();
            if (!tracker->loadModel(config.siamfcFeatureModelPath, config.siamfcTrackingModelPath)) {
                throw std::runtime_error("Failed to load SiamFCPP tracker models");
            }
            return tracker;
        }
            
        default:
            throw std::runtime_error("Unknown tracker type");
    }
}

void TrackerManager::initialize(std::unique_ptr<TrackerInterface> tracker, bool showTrackingPath) {
    tracker_ = std::move(tracker);
    showTrackingPath_ = showTrackingPath;
    isTracking_ = false;
    trackedClassId_ = -1;
    trackingPath_.clear();
}

bool TrackerManager::start(const cv::Mat& frame, const cv::Rect& box, int classId, const std::vector<std::string>& classNames) {
    try {
        if (tracker_ && tracker_->init(frame, box)) {
            isTracking_ = true;
            lastTrackBox_ = box;
            trackedClassId_ = classId;
            trackingPath_.clear();
            
            if (showTrackingPath_) {
                trackingPath_.push_back(cv::Point(box.x + box.width / 2, box.y + box.height / 2));
            }
            
            return true;
        } else {
            LOG_ERROR("Tracker initialization failed");
            isTracking_ = false;
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Tracker initialization failed: {}", e.what());
        isTracking_ = false;
        return false;
    }
}

void TrackerManager::update(const cv::Mat& frame) {
    try {
        if (tracker_) {
            cv::Rect newTrackBox = tracker_->update(frame);
            
            bool trackerValid = newTrackBox.width > 0 && newTrackBox.height > 0 && tracker_->isInitialized();
            if (!trackerValid) {
                isTracking_ = false;
                trackingPath_.clear();
                trackedClassId_ = -1;
            } else {
                lastTrackBox_ = newTrackBox;
                
                if (showTrackingPath_) {
                    trackingPath_.push_back(cv::Point(lastTrackBox_.x + lastTrackBox_.width / 2, lastTrackBox_.y + lastTrackBox_.height / 2));
                    if (trackingPath_.size() > MAX_PATH_POINTS) {
                        trackingPath_.erase(trackingPath_.begin());
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Tracker update failed: {}", e.what());
        isTracking_ = false;
        trackingPath_.clear();
        trackedClassId_ = -1;
    }
} 