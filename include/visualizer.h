#pragma once

#include "include/model.h"
#include "include/navigation_unit.h" // For ControlOutputs
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <map>
#include <mutex>

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
                           const ControlOutputs* controlOutputs = nullptr,
                           uint32_t flightMode = 0,
                           bool mavlinkConnected = false,
                           float centeringRadius = 50.0f);

    void displayFrame(const cv::Mat& frame, const std::string& windowName = "Live View");
    
    // Combined visualization - shows detection and tracking side by side
    void displayCombinedView(const cv::Mat& detectionFrame, const cv::Mat& trackingFrame, 
                           const std::string& windowName = "Detection & Tracking");

private:
    void setupWindow(const std::string& windowName);
    cv::Scalar getClassColor(const std::string& className);
    std::string getStatusText(const std::string& className);
    std::string getFlightModeName(uint32_t flightMode);
    
    std::map<std::string, bool> m_windows;
    
    // Frame storage for combined view
    cv::Mat m_detectionFrame;
    cv::Mat m_trackingFrame;
    std::mutex m_frameMutex;
    bool m_enableCombinedView;

public:
    // Combined view control
    void enableCombinedView(bool enable = true) { m_enableCombinedView = enable; }
    void updateDetectionFrame(const cv::Mat& frame);
    void updateTrackingFrame(const cv::Mat& frame);
    void showCombinedView();
}; 