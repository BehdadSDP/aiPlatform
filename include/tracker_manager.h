#pragma once

#include "tracker_interface.h"
#include "include/model_manager.h"
#include "include/control_unit.h"
#include <opencv2/opencv.hpp>
#include <memory>
#include <atomic>

class TrackerManager {
public:
    explicit TrackerManager(std::unique_ptr<TrackerInterface> tracker, bool showTrackingPath = true);
    
    void runTrackingLoop(std::atomic<bool>& running, ModelManager& modelManager, 
                        ControlUnit& controlUnit);

private:
    void initializeTracker(const cv::Mat& frame, const cv::Rect& bbox, int classId,
                          const std::vector<std::string>& classNames);
    void updateTracker(const cv::Mat& frame, ControlUnit& controlUnit);
    void visualizeTracking(const cv::Mat& frame);
    
    std::unique_ptr<TrackerInterface> tracker_;
    bool isTracking_ = false;
    cv::Rect lastTrackBox_;
    bool showTrackingPath_ = true;
    
    // Variables for path tracking visualization
    std::vector<cv::Point> trackingPath_;
    static const int MAX_PATH_POINTS = 50;
    cv::Scalar pathColor_ = cv::Scalar(255, 100, 0);
}; 