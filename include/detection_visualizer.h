#pragma once

#include "include/model.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

// Forward declaration to avoid circular includes
class TrafficIntensityManager;

class DetectionVisualizer {
public:
    DetectionVisualizer();
    
    void visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, 
                           const std::vector<std::string>& classNames);
    
    // New method that considers traffic intensity polygons
    void visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, 
                           const std::vector<std::string>& classNames,
                           const TrafficIntensityManager& trafficManager);

private:
    void setupWindow();
    cv::Scalar getClassColor(const std::string& className);
    std::string getStatusText(const std::string& className);
    bool isDetectionInTrafficPolygons(const model::Detection& detection, 
                                     const TrafficIntensityManager& trafficManager);
    
    bool windowCreated_ = false;
}; 