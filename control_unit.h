#pragma once
#include "shared_data.h"
#include <opencv2/opencv.hpp>
#include <atomic>

class ControlUnit {
public:
    ControlUnit() = default;
    void setDetectionFrameInterval(int interval);
    bool shouldDetect() const;
    void incrementDetectionFrameCounter() {
            std::lock_guard<std::mutex> lock(counterMutex_);
            detectionFrameCounter_++;
        }
    void resetDetectionFrameInterval();
    int getDetectionFrameCounter() const { return detectionFrameCounter_; }

private:
    std::atomic<int> detectionFrameInterval_{1};
    std::atomic<int> trackingFrameInterval_{1};
    int detectionFrameCounter_{0};
    int trackingFrameCounter_{0};
    mutable std::mutex counterMutex_;
};
