#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <string>
#include "tracker_interface.h"

class VitTracker : public TrackerInterface
{
public:
    // Constructor takes the ONNX model path
    explicit VitTracker(const std::string& onnxPath);

    // TrackerInterface implementation
    bool init(const cv::Mat& frame, const cv::Rect& initBox) override;
    cv::Rect update(const cv::Mat& frame) override;
    bool isInitialized() const override { return initialized_; }
    float getLastConfidence() const override { return trackScore_; }

private:
    cv::TrackerVit::Params params_;         // Tracker parameters
    cv::Ptr<cv::TrackerVit> tracker_;      // TrackerVit instance
    cv::Rect trackedBox_;                   // Current bounding box
    bool initialized_ = false;              // Initialization flag
    float trackScore_ = 1.0f;               // Tracking confidence
};
