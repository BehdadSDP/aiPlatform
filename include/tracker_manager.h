#pragma once

#include "tracker_interface.h"
#include <memory>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

enum class TrackerType {
    VIT_TRACKER = 0,
    SIAMFC_TRACKER = 1
};

struct TrackerConfig {
    TrackerType type;
    std::string vitModelPath;
    std::string siamfcFeatureModelPath;
    std::string siamfcTrackingModelPath;
};

class TrackerManager {
public:
    TrackerManager();
    static std::unique_ptr<TrackerInterface> createTracker(const TrackerConfig& config);

    void initialize(std::unique_ptr<TrackerInterface> tracker, bool showTrackingPath = true);
    bool start(const cv::Mat& frame, const cv::Rect& box, int classId, const std::vector<std::string>& classNames);
    void update(const cv::Mat& frame);

    bool isTracking() const { return isTracking_; }
    cv::Rect getLastTrackBox() const { return lastTrackBox_; }
    int getTrackedClassId() const { return trackedClassId_; }
    const std::vector<cv::Point>& getTrackingPath() const { return trackingPath_; }

private:
    std::unique_ptr<TrackerInterface> tracker_;
    bool isTracking_ = false;
    cv::Rect lastTrackBox_;
    bool showTrackingPath_ = true;
    int trackedClassId_ = -1;
    std::vector<cv::Point> trackingPath_;
    static const int MAX_PATH_POINTS = 50;
}; 