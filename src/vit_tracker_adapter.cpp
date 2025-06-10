#include "include/vit_tracker_adapter.h"

VitTrackerAdapter::VitTrackerAdapter(const std::string& modelPath) : tracker_(modelPath) {}

void VitTrackerAdapter::init(const cv::Mat& frame, const cv::Rect& initBox) {
    tracker_.init(frame, initBox);
}

cv::Rect VitTrackerAdapter::update(const cv::Mat& frame) {
    return tracker_.update(frame);
}

bool VitTrackerAdapter::isInitialized() const {
    return tracker_.isInitialized();
} 