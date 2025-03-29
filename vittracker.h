#pragma once
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>
#include <string>

class VitTracker
{
public:
    // Load the .onnx model in the constructor
    explicit VitTracker(const std::string& onnxPath);

    // Initialize with the first frame and bounding box
    void init(const cv::Mat& frame, const cv::Rect& initBox);

    // Update the tracking on a subsequent frame
    // Returns the new bounding box
    cv::Rect update(const cv::Mat& frame);

    // (Optional) For advanced trackers that measure confidence
    // or might degrade over time.
    float getTrackingScore() const { return trackScore_; }

    bool isInitialized() const { return initialized_; }

private:
    cv::dnn::Net vitNet_;
    cv::Rect trackedBox_;
    bool initialized_ = false;
    float trackScore_ = 1.0f;

    // Internal buffers, etc., that ViTTrack might need
    // This is a placeholder; the real model may require
    // more extensive data structures for cropping/resizing,
    // historical info, etc.
};
