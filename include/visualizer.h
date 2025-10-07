#pragma once

#include "include/model.h"
#include "include/navigation_unit.h" // For ControlOutputs
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <map>

class Visualizer {
public:
    Visualizer();
    
    // Detection visualization
    void visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, 
                           const std::vector<std::string>& classNames);

    // Tracking visualization
    void visualizeTracking(cv::Mat& frame, bool isTracking, const cv::Rect& trackedBox, 
                           int trackedClassId, const std::vector<std::string>& classNames,
                           const std::vector<cv::Point>& trackingPath,
                           const ControlOutputs* controlOutputs = nullptr);

    void displayFrame(const cv::Mat& frame, const std::string& windowName = "Live View");

private:
    void setupWindow(const std::string& windowName);
    cv::Scalar getClassColor(const std::string& className);
    std::string getStatusText(const std::string& className);
    
    std::map<std::string, bool> m_windows;
}; 