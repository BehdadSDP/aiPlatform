#include "control_unit.h"
#include <iostream>

ControlUnit::Action ControlUnit::decideAction(SingleObjectData& sharedData, bool isTracking, const cv::Rect& lastTrackBox) const {
    bool yoloValid = false;
    cv::Rect yoloBox;

    {
        std::lock_guard<std::mutex> lock(sharedData.mtx);
        yoloValid = sharedData.detection.valid;
        if (yoloValid) {
            yoloBox = sharedData.detection.box;
        }
    }

    if (!isTracking && yoloValid) {
        std::cout << "Control Unit: Decision - INITIALIZE with YOLO box " << yoloBox << std::endl;
        return Action::INITIALIZE;
    }

    if (isTracking) {
        if (lastTrackBox.width <= 0 || lastTrackBox.height <= 0) {
            if (yoloValid) {
                std::cout << "Control Unit: Decision - REINITIALIZE with YOLO box " << yoloBox << std::endl;
                return Action::REINITIALIZE;
            } else {
                std::cout << "Control Unit: Decision - STOP (tracker failed, no YOLO detection)" << std::endl;
                return Action::STOP;
            }
        }
        std::cout << "Control Unit: Decision - CONTINUE tracking" << std::endl;
        return Action::CONTINUE;
    }

    return Action::STOP;
}

void ControlUnit::setDetectionFrameInterval(int interval) {
    detectionFrameInterval_ = (interval > 0) ? interval : 1;
    detectionFrameCounter_ = 0;
    std::cout << "Control Unit: Detection frame interval set to " << detectionFrameInterval_ << std::endl;
}

void ControlUnit::setTrackingFrameInterval(int interval) {
    trackingFrameInterval_ = (interval > 0) ? interval : 1;
    trackingFrameCounter_ = 0;
    std::cout << "Control Unit: Tracking frame interval set to " << trackingFrameInterval_ << std::endl;
}

bool ControlUnit::shouldDetect() const {
    int counter = detectionFrameCounter_++;
    return (counter % detectionFrameInterval_ == 0);
}

bool ControlUnit::shouldTrack() const {
    int counter = trackingFrameCounter_++;
    return (counter % trackingFrameInterval_ == 0);
}

void ControlUnit::setDetectionMode(Mode mode) {
    detectionMode_ = mode;
    detectionFrameCounter_ = 0; // Reset counter on mode change
    std::cout << "Control Unit: Detection mode set to " << static_cast<int>(mode) << std::endl;
}

void ControlUnit::setTrackingMode(Mode mode) {
    trackingMode_ = mode;
    trackingFrameCounter_ = 0; // Reset counter on mode change
    std::cout << "Control Unit: Tracking mode set to " << static_cast<int>(mode) << std::endl;
}
