#pragma once

#include <opencv2/opencv.hpp>

class TrackerInterface {
public:
    virtual ~TrackerInterface() = default;
    virtual void init(const cv::Mat& frame, const cv::Rect& initBox) = 0;
    virtual cv::Rect update(const cv::Mat& frame) = 0;
    virtual bool isInitialized() const = 0;
    
    // Optional method for SiamFCPP-style initialization
    virtual void model_initializer(const cv::Mat& frame, const cv::Rect& bbox) {
        init(frame, bbox);
    }
    
    // Add method to get last confidence
    virtual float getLastConfidence() const {
        return 0.0f;
    }
}; 