#include "include/visualizer.h"
#include <map>

Visualizer::Visualizer() {
    m_enableCombinedView = false;
}

void Visualizer::visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, 
                                             const std::vector<std::string>& classNames) {
    if (frame.empty()) return;

    for (const auto& det : detections) {
        if (det.confidence > 0.15f) {
            
            std::string className = (det.classId >= 0 && det.classId < static_cast<int>(classNames.size())) ?
                                   classNames[det.classId] : "Unknown";
            
            cv::Scalar boxColor = getClassColor(className);
            std::string statusText = getStatusText(className);
            
            cv::rectangle(frame, det.box, boxColor, 1);

            std::string label = className + ": " + std::to_string(int(det.confidence * 100)) + "%" + statusText;

            int baseline = 0;
            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
            cv::rectangle(frame,
                         cv::Point(det.box.x, det.box.y - textSize.height - 10),
                         cv::Point(det.box.x + textSize.width, det.box.y),
                         boxColor, -1);

            cv::putText(frame, label,
                       cv::Point(det.box.x, det.box.y - 5),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        }
    }
}

void Visualizer::visualizeTracking(cv::Mat& frame, bool isTracking, const cv::Rect& trackedBox, 
                                   int trackedClassId, const std::vector<std::string>& classNames,
                                   const std::vector<cv::Point>& trackingPath,
                                   const ControlOutputs* controlOutputs,
                                   uint32_t flightMode,
                                   bool mavlinkConnected,
                                   float centeringRadius) {
    if (frame.empty()) return;

    // Draw center point of camera frame
    cv::Point frameCenter(frame.cols / 2, 2* (frame.rows / 3));
    cv::circle(frame, frameCenter, 5, cv::Scalar(255, 255, 255), -1); // White center point
    cv::circle(frame, frameCenter, 8, cv::Scalar(0, 0, 0), 2); // Black border

    if (isTracking) {
        cv::rectangle(frame, trackedBox, cv::Scalar(0, 255, 0), 2);
        
        // Draw center point of tracked object bounding box
        cv::Point objectCenter(trackedBox.x + trackedBox.width / 2, trackedBox.y + trackedBox.height / 2);
        cv::circle(frame, objectCenter, 5, cv::Scalar(0, 255, 0), -1); // Green center point
        cv::circle(frame, objectCenter, 8, cv::Scalar(0, 0, 0), 2); // Black border
        
        // Draw tolerance circle around object center using the configured radius
        cv::circle(frame, objectCenter, static_cast<int>(centeringRadius), cv::Scalar(255, 0, 255), 2); // Magenta circle
        
        // Draw line between frame center and object center
        cv::line(frame, frameCenter, objectCenter, cv::Scalar(255, 255, 0), 2); // Yellow line
        
        std::string className = (trackedClassId >= 0 && trackedClassId < static_cast<int>(classNames.size())) ?
                               classNames[trackedClassId] : "Unknown";
        std::string label = "Tracking: " + className;
        cv::putText(frame, label, cv::Point(trackedBox.x, trackedBox.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
        
        // Display control outputs if available
        if (controlOutputs) {
            std::string controlText = "Roll: " + std::to_string(controlOutputs->roll_output) + 
                                    " Pitch: " + std::to_string(controlOutputs->pitch_output);
            cv::putText(frame, controlText, cv::Point(10, 30),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);
            
            std::string errorText = "Filtered Error X: " + std::to_string(static_cast<int>(controlOutputs->error.x)) + 
                                  " Y: " + std::to_string(static_cast<int>(controlOutputs->error.y));
            cv::putText(frame, errorText, cv::Point(10, 60),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);
            
            // Display RC command status
            std::string rcStatusText = controlOutputs->rc_commands_sent ? "RC Commands: ACTIVE" : "RC Commands: CENTERED (SUPPRESSED)";
            cv::Scalar rcStatusColor = controlOutputs->rc_commands_sent ? cv::Scalar(0, 255, 255) : cv::Scalar(0, 255, 0); // Yellow for active, Green for suppressed
            cv::putText(frame, rcStatusText, cv::Point(10, 90),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, rcStatusColor, 2);
        }
        
        // Display MAVLink connection status and flight mode
        if (mavlinkConnected) {
            std::string flightModeText = "Flight Mode: " + getFlightModeName(flightMode);
            cv::Scalar flightModeColor = (flightMode == 2) ? cv::Scalar(0, 255, 0) : cv::Scalar(200, 200, 200); // Green for ALT_HOLD, Gray for others
            cv::putText(frame, flightModeText, cv::Point(10, 120),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, flightModeColor, 2);
            
            // Display MAVLink status
            cv::putText(frame, "MAVLink: CONNECTED", cv::Point(10, 150),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
        } else {
            cv::putText(frame, "MAVLink: DISCONNECTED", cv::Point(10, 150),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
        }
    }

    for (size_t i = 1; i < trackingPath.size(); ++i) {
        cv::line(frame, trackingPath[i-1], trackingPath[i], cv::Scalar(255, 100, 0), 2);
    }
}


void Visualizer::displayFrame(const cv::Mat& frame, const std::string& windowName) {
    setupWindow(windowName);
    cv::imshow(windowName, frame);
    cv::waitKey(1);
}

void Visualizer::displayCombinedView(const cv::Mat& detectionFrame, const cv::Mat& trackingFrame, 
                                   const std::string& windowName) {
    if (detectionFrame.empty() || trackingFrame.empty()) {
        std::cerr << "Warning: One or both frames are empty for combined view" << std::endl;
        return;
    }
    
    // Ensure both frames have the same height for proper concatenation
    cv::Mat resizedDetection, resizedTracking;
    int targetHeight = std::min(detectionFrame.rows, trackingFrame.rows);
    
    // Resize detection frame maintaining aspect ratio
    float detectionRatio = static_cast<float>(detectionFrame.cols) / detectionFrame.rows;
    int detectionWidth = static_cast<int>(targetHeight * detectionRatio);
    cv::resize(detectionFrame, resizedDetection, cv::Size(detectionWidth, targetHeight));
    
    // Resize tracking frame maintaining aspect ratio  
    float trackingRatio = static_cast<float>(trackingFrame.cols) / trackingFrame.rows;
    int trackingWidth = static_cast<int>(targetHeight * trackingRatio);
    cv::resize(trackingFrame, resizedTracking, cv::Size(trackingWidth, targetHeight));
    
    // Create combined frame
    cv::Mat combinedFrame;
    cv::hconcat(resizedDetection, resizedTracking, combinedFrame);
    
    // Add labels to distinguish the views
    cv::putText(combinedFrame, "DETECTION", cv::Point(10, 30),
               cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
    cv::putText(combinedFrame, "TRACKING", cv::Point(detectionWidth + 10, 30),
               cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
    
    // Draw vertical separator line
    cv::line(combinedFrame, cv::Point(detectionWidth, 0), 
             cv::Point(detectionWidth, targetHeight), cv::Scalar(128, 128, 128), 2);
    
    setupWindow(windowName);
    cv::imshow(windowName, combinedFrame);
    cv::waitKey(1);
}

void Visualizer::setupWindow(const std::string& windowName) {
    if (m_windows.find(windowName) == m_windows.end()) {
        cv::namedWindow(windowName, cv::WINDOW_NORMAL);
        
        // Set larger window size for combined view
        if (windowName == "Detection & Tracking") {
            cv::resizeWindow(windowName, 1600, 600);  // Wider for side-by-side
        } else {
            cv::resizeWindow(windowName, 800, 600);
        }
        
        m_windows[windowName] = true;
    }
}

cv::Scalar Visualizer::getClassColor(const std::string& className) {
    if (className == "helmet" || className == "hardhat") {
        return cv::Scalar(0, 255, 0);
    } else if (className == "no-helmet" || className == "head") {
        return cv::Scalar(0, 0, 255);
    } else if (className == "person") {
        return cv::Scalar(255, 165, 0);
    } else if (className == "vehicle" || className == "car" || className == "truck" || className == "bus" || className == "motorcycle" || className == "bicycle") {
        return cv::Scalar(255, 0, 0);
    } else {
        return cv::Scalar(0, 255, 255);
    }
}

std::string Visualizer::getStatusText(const std::string& className) {
    if (className == "helmet" || className == "hardhat") {
        return " ✓ SAFE";
    } else if (className == "no-helmet" || className == "head") {
        return " ⚠ VIOLATION";
    } else {
        return "";
    }
}

void Visualizer::updateDetectionFrame(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (!frame.empty()) {
        m_detectionFrame = frame.clone();
    }
}

void Visualizer::updateTrackingFrame(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (!frame.empty()) {
        m_trackingFrame = frame.clone();
    }
}

void Visualizer::showCombinedView() {
    if (!m_enableCombinedView) return;
    
    std::lock_guard<std::mutex> lock(m_frameMutex);
    
    // Check if both frames are available
    if (m_detectionFrame.empty() && m_trackingFrame.empty()) {
        return; // No frames to display
    }
    
    // Handle case where only one frame is available
    cv::Mat detectionDisplay, trackingDisplay;
    
    if (m_detectionFrame.empty()) {
        // Create placeholder for detection
        detectionDisplay = cv::Mat::zeros(480, 640, CV_8UC3);
        cv::putText(detectionDisplay, "DETECTION: Waiting for data...", 
                   cv::Point(50, 240), cv::FONT_HERSHEY_SIMPLEX, 1.0, 
                   cv::Scalar(128, 128, 128), 2);
    } else {
        detectionDisplay = m_detectionFrame.clone();
    }
    
    if (m_trackingFrame.empty()) {
        // Create placeholder for tracking
        trackingDisplay = cv::Mat::zeros(480, 640, CV_8UC3);
        cv::putText(trackingDisplay, "TRACKING: Waiting for data...", 
                   cv::Point(50, 240), cv::FONT_HERSHEY_SIMPLEX, 1.0, 
                   cv::Scalar(128, 128, 128), 2);
    } else {
        trackingDisplay = m_trackingFrame.clone();
    }
    
    displayCombinedView(detectionDisplay, trackingDisplay);
}

std::string Visualizer::getFlightModeName(uint32_t flightMode) {
    // ArduPilot/PX4 flight mode numbers
    // Common ArduPilot modes for copter:
    switch(flightMode) {
        case 0: return "STABILIZE";
        case 1: return "ACRO";
        case 2: return "ALT_HOLD";
        case 3: return "AUTO";
        case 4: return "GUIDED";
        case 5: return "LOITER";
        case 6: return "RTL";
        case 7: return "CIRCLE";
        case 9: return "LAND";
        case 11: return "DRIFT";
        case 13: return "SPORT";
        case 14: return "FLIP";
        case 15: return "AUTOTUNE";
        case 16: return "POSHOLD";
        case 17: return "BRAKE";
        case 18: return "THROW";
        case 19: return "AVOID_ADSB";
        case 20: return "GUIDED_NOGPS";
        case 21: return "SMART_RTL";
        case 22: return "FLOWHOLD";
        case 23: return "FOLLOW";
        case 24: return "ZIGZAG";
        case 25: return "SYSTEMID";
        case 26: return "AUTOROTATE";
        case 27: return "AUTO_RTL";
        default: return "UNKNOWN(" + std::to_string(flightMode) + ")";
    }
}
