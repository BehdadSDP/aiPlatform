#pragma once

#include "tracker_interface.h"
#include "include/siamfc_pp_tracker.h"
#include <memory>
#include <string>

class SiamFCPPAdapter : public TrackerInterface {
public:
    explicit SiamFCPPAdapter(const std::string& featureModelPath, const std::string& trackModelPath);

    void init(const cv::Mat& frame, const cv::Rect& initBox) override;
    cv::Rect update(const cv::Mat& frame) override;
    bool isInitialized() const override;
    void model_initializer(const cv::Mat& frame, const cv::Rect& bbox) override;
    float getLastConfidence() const override;

private:
    std::unique_ptr<SiamFCPPTracker2> tracker_;
    bool initialized_ = false;
    float lastConfidence_ = 0.0f;
    cv::Rect lastValidResult_;
    int failureCount_ = 0;
}; 
