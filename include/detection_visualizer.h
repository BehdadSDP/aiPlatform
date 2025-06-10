#pragma once

#include "include/model.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

class DetectionVisualizer {
public:
    DetectionVisualizer();
    
    void visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, 
                           const std::vector<std::string>& classNames);

private:
    void setupWindow();
    cv::Scalar getClassColor(const std::string& className);
    std::string getStatusText(const std::string& className);
    
    bool windowCreated_ = false;
}; 