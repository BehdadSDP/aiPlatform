#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include "../tracker_interface.h"

/**
 * @brief CSRT (Discriminative Correlation Filter with Channel and Spatial Reliability) Tracker
 * 
 * OpenCV implementation of CSRT tracker - accurate and robust tracker
 * that uses spatial reliability maps and channel reliability.
 * Good balance between speed and accuracy.
 */
class CSRTTracker : public TrackerInterface
{
public:
    CSRTTracker();
    ~CSRTTracker() override = default;

    // TrackerInterface implementation
    bool init(const cv::Mat& frame, const cv::Rect& initBox) override;
    cv::Rect update(const cv::Mat& frame) override;
    bool isInitialized() const override { return initialized_; }
    float getLastConfidence() const override { return confidence_; }

private:
    cv::Ptr<cv::TrackerCSRT> tracker_;  // OpenCV CSRT tracker instance
    cv::Rect trackedBox_;               // Current bounding box
    bool initialized_ = false;          // Initialization flag
    float confidence_ = 1.0f;           // Tracking confidence
    
    // Tracking quality monitoring
    int consecutiveFailures_ = 0;       // Count of consecutive tracking failures
    static const int MAX_FAILURES = 3;  // Maximum failures before declaring tracking lost
};
