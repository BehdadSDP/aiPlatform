#include "vittracker.h"
#include <stdexcept>
#include <iostream>

VitTracker::VitTracker(const std::string& onnxPath)
{
    // Load the ViTTrack .onnx model
    vitNet_ = cv::dnn::readNetFromONNX(onnxPath);
    if (vitNet_.empty()) {
        throw std::runtime_error("Failed to load ViTTrack model from " + onnxPath);
    }
    vitNet_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    vitNet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
}

void VitTracker::init(const cv::Mat& frame, const cv::Rect& initBox)
{
    trackedBox_ = initBox;

    // Some trackers do an initialization pass (template creation, etc.)
    // For ViTTrack, you often feed the first frame/crop bounding box
    // to the model in a special "init" forward pass. Pseudocode here:
    //   1) Crop the region from `frame(initBox)`
    //   2) Possibly resize to required input size
    //   3) Set input to vitNet_, run forward for "init"

    // Pseudo:
    // cv::Mat roi = frame(initBox).clone();
    // cv::Mat blob = cv::dnn::blobFromImage(roi, ...);
    // vitNet_.setInput(blob, "template");
    // vitNet_.forward(); // e.g. "init"

    initialized_ = true;
    trackScore_ = 1.0f; // Reset track confidence
}

cv::Rect VitTracker::update(const cv::Mat& frame)
{
    // 1) Crop search area from frame around trackedBox_
    // 2) Construct the input blob for ViT
    // 3) forward pass the search region
    // 4) parse the output to get updated bounding box
    // Below is pseudocode. In practice, refer to official ViTTrack demos.

    if (!initialized_) {
        return cv::Rect();
    }

    // Example: trackScore_ might degrade if the model is uncertain
    // or if the bounding box is off-screen, etc.
    // For demonstration:
    trackScore_ -= 0.001f;

    // Pseudocode for forward:
    // cv::Mat searchRegion = getCropAroundBox(frame, trackedBox_);
    // cv::Mat blob = cv::dnn::blobFromImage(searchRegion, 1.0, modelInputSize, ...);
    // vitNet_.setInput(blob, "search");
    // cv::Mat out = vitNet_.forward("output");
    //
    // parse out -> newBox coords relative to the search region
    // convert them back to frame coords
    // trackedBox_ = ... newBox

    // For now, let's pretend we just shift it slightly
    // (purely for demonstration).
    trackedBox_.x += 1;
    trackedBox_.y += 1;

    return trackedBox_;
}
