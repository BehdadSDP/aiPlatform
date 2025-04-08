#include "control_unit.h"
#include <iostream>

ControlUnit::Action ControlUnit::decideAction(SingleObjectData& sharedData, bool isTracking, const cv::Rect& lastTrackBox) const {
    bool yoloValid = false;
    cv::Rect yoloBox;
    bool newDetection = false;

    {
        std::lock_guard<std::mutex> lock(sharedData.mtx);
        yoloValid = sharedData.detection.valid;
        newDetection = sharedData.detection.newDetection;
        if (yoloValid) {
            yoloBox = sharedData.detection.box;
        }
    }

    if (!isTracking && yoloValid && newDetection && detectionMode_ == Mode::RUN) {
        const_cast<ControlUnit*>(this)->detectionMode_ = Mode::STANDBY;
        const_cast<ControlUnit*>(this)->trackingMode_ = Mode::RUN;
        return Action::INITIALIZE;
    }

    if (isTracking && trackingMode_ == Mode::RUN) {
        if (lastTrackBox.width <= 0 || lastTrackBox.height <= 0) {
            const_cast<ControlUnit*>(this)->trackingMode_ = Mode::STANDBY;
            const_cast<ControlUnit*>(this)->detectionMode_ = Mode::RUN;
            return Action::STOP;
        }
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
    return (detectionFrameCounter_ % detectionFrameInterval_ == 0);
}

bool ControlUnit::shouldTrack() const {
    return (trackingFrameCounter_ % trackingFrameInterval_ == 0);
}

void ControlUnit::setDetectionMode(Mode mode) {
    detectionMode_ = mode;
    detectionFrameCounter_ = 0;
    std::cout << "Control Unit: Detection mode set to " << static_cast<int>(mode) << std::endl;
}

void ControlUnit::setTrackingMode(Mode mode) {
    trackingMode_ = mode;
    trackingFrameCounter_ = 0;
    std::cout << "Control Unit: Tracking mode set to " << static_cast<int>(mode) << std::endl;
}
