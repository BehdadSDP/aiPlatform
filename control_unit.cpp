#include "control_unit.h"
#include <iostream>

void ControlUnit::setDetectionFrameInterval(int interval) {
    detectionFrameInterval_ = (interval > 0) ? interval : 1;
    detectionFrameCounter_ = 0; // Reset when interval changes
    std::cout << "Control Unit: Detection frame interval set to " << detectionFrameInterval_ << std::endl;
}

void ControlUnit::resetDetectionFrameInterval() {
    std::lock_guard<std::mutex> lock(counterMutex_);
    detectionFrameCounter_ = 0;
}

bool ControlUnit::shouldDetect() const {
    std::lock_guard<std::mutex> lock(counterMutex_);
    return (detectionFrameCounter_ % detectionFrameInterval_ == 0);
}

