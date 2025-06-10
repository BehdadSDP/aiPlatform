#pragma once

#include "tracker_interface.h"
#include "include/vittracker.h"
#include <string>

class VitTrackerAdapter : public TrackerInterface {
public:
    explicit VitTrackerAdapter(const std::string& modelPath);

    void init(const cv::Mat& frame, const cv::Rect& initBox) override;
    cv::Rect update(const cv::Mat& frame) override;
    bool isInitialized() const override;

private:
    VitTracker tracker_;
}; 
