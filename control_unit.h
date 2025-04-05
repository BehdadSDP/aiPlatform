#pragma once
#include "shared_data.h"
#include <opencv2/opencv.hpp>

class ControlUnit {
public:
    ControlUnit() = default;

    enum class Action {
        INITIALIZE,
        CONTINUE,
        REINITIALIZE,
        STOP
    };

    // Remove const from sharedData to allow mutex locking
    Action decideAction(SingleObjectData& sharedData, bool isTracking, const cv::Rect& lastTrackBox) const;
};
