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

    enum class Mode {
        RUN,
        STANDBY
    };

    Action decideAction(SingleObjectData& sharedData, bool isTracking, const cv::Rect& lastTrackBox) const;

    void setDetectionFrameInterval(int interval);
    void setTrackingFrameInterval(int interval);
    bool shouldDetect() const;
    bool shouldTrack() const;

    void setDetectionMode(Mode mode);
    void setTrackingMode(Mode mode);
    Mode getDetectionMode() const { return detectionMode_; }
    Mode getTrackingMode() const { return trackingMode_; }

    void incrementDetectionFrameCounter() { detectionFrameCounter_++; }
    void incrementTrackingFrameCounter() { trackingFrameCounter_++; }

private:
    std::atomic<Mode> detectionMode_{Mode::RUN}; // YOLO starts in RUN
    std::atomic<Mode> trackingMode_{Mode::STANDBY}; // Tracker starts in STANDBY
    std::atomic<int> detectionFrameInterval_{1};
    std::atomic<int> trackingFrameInterval_{1};
    std::atomic<int> detectionFrameCounter_{0};
    std::atomic<int> trackingFrameCounter_{0};
};
