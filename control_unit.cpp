#include "control_unit.h"
#include <iostream>

ControlUnit::Action ControlUnit::decideAction(SingleObjectData& sharedData, bool isTracking, const cv::Rect& lastTrackBox) const {
    bool yoloValid = false;
    cv::Rect yoloBox;

    // Lock the mutex (now works because sharedData is non-const)
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
