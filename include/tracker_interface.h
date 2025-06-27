#pragma once

#include <opencv2/opencv.hpp>

class TrackerInterface {
public:
    virtual ~TrackerInterface() = default;
    
    // Core tracking methods
    virtual bool init(const cv::Mat& frame, const cv::Rect& initBox) = 0;
    virtual cv::Rect update(const cv::Mat& frame) = 0;
    virtual bool isInitialized() const = 0;
    virtual float getLastConfidence() const = 0;
}; 