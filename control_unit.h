#pragma once
#include "shared_data.h"
#include <opencv2/opencv.hpp>
#include <atomic>

class ControlUnit {
public:
    ControlUnit() = default;

    enum class Action {
        INITIALIZE,
        CONTINUE,
        REINITIALIZE,
        STOP
    };

    // Existing decision method
    Action decideAction(SingleObjectData& sharedData, bool isTracking, const cv::Rect& lastTrackBox) const;

    // New methods for frame speed control
    void setDetectionFrameInterval(int interval);
    void setTrackingFrameInterval(int interval);
    bool shouldDetect() const;
    bool shouldTrack() const;

private:
    std::atomic<int> detectionFrameInterval_{1}; // Default: process every frame
    std::atomic<int> trackingFrameInterval_{1};  // Default: process every frame
    mutable std::atomic<int> detectionFrameCounter_{0}; // Counter for detection
    mutable std::atomic<int> trackingFrameCounter_{0};  // Counter for tracking
};
