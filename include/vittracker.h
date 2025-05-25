#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <string>

class VitTracker
{
public:
    // Constructor takes the ONNX model path
    explicit VitTracker(const std::string& onnxPath);

    // Initialize with the first frame and bounding box
    void init(const cv::Mat& frame, const cv::Rect& initBox);

    // Update tracking on a subsequent frame, returns the new bounding box
    cv::Rect update(const cv::Mat& frame);

    // Get tracking confidence score
    float getTrackingScore() const { return trackScore_; }

    bool isInitialized() const { return initialized_; }

private:
    cv::TrackerVit::Params params_;         // Tracker parameters
    cv::Ptr<cv::TrackerVit> tracker_;      // TrackerVit instance
    cv::Rect trackedBox_;                   // Current bounding box
    bool initialized_ = false;              // Initialization flag
    float trackScore_ = 1.0f;               // Tracking confidence
};
