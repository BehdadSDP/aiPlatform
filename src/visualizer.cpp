#include "include/visualizer.h"
#include "include/logger.h"
#include <map>

Visualizer::Visualizer() {
    m_enableCombinedView = false;
}

void Visualizer::visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, 
                                             const std::vector<std::string>& classNames,
                                             const std::string& modelName) {
    if (frame.empty()) return;

    // Create black background bar at the top for model information
    int barHeight = 40;
    cv::rectangle(frame, cv::Point(0, 0), cv::Point(frame.cols, barHeight), cv::Scalar(0, 0, 0), -1);
    
    // Display model name if provided
    if (!modelName.empty()) {
        float fontSize = 0.7;
        int fontThickness = 2;
        std::string modelText = "Model: " + modelName;
        cv::putText(frame, modelText, cv::Point(10, 28),
                   cv::FONT_HERSHEY_SIMPLEX, fontSize, cv::Scalar(255, 255, 255), fontThickness);
    }

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

void Visualizer::visualizeDetectionsWithNumbers(cv::Mat& frame, const std::vector<model::Detection>& detections, 
                                               const std::vector<std::string>& classNames,
                                               const std::string& modelName) {
    if (frame.empty()) return;

    // Create black background bar at the top for model information
    int barHeight = 40;
    cv::rectangle(frame, cv::Point(0, 0), cv::Point(frame.cols, barHeight), cv::Scalar(0, 0, 0), -1);
    
    // Display model name if provided
    if (!modelName.empty()) {
        float fontSize = 0.7;
        int fontThickness = 2;
        std::string modelText = "Model: " + modelName;
        cv::putText(frame, modelText, cv::Point(10, 28), cv::FONT_HERSHEY_SIMPLEX, fontSize, cv::Scalar(255, 255, 255), fontThickness);
    }

    // Filter detections with confidence > 0.15 and add numbers
    std::vector<model::Detection> validDetections;
    for (const auto& det : detections) {
        if (det.confidence > 0.15f) {
            validDetections.push_back(det);
        }
    }

    for (size_t i = 0; i < validDetections.size(); ++i) {
        const auto& det = validDetections[i];
        
        std::string className = (det.classId >= 0 && det.classId < static_cast<int>(classNames.size())) ?
                               classNames[det.classId] : "Unknown";
        
        cv::Scalar boxColor = getClassColor(className);
        std::string statusText = getStatusText(className);
        
        // Draw bounding box with thicker border for visibility
        cv::rectangle(frame, det.box, boxColor, 3);

        // Create label with number
        std::string label = "[" + std::to_string(i + 1) + "] " + className + ": " + 
                           std::to_string(int(det.confidence * 100)) + "%" + statusText;

        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
        cv::rectangle(frame,
                     cv::Point(det.box.x, det.box.y - textSize.height - 10),
                     cv::Point(det.box.x + textSize.width, det.box.y),
                     boxColor, -1);

        cv::putText(frame, label,
                   cv::Point(det.box.x, det.box.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        
        // Draw large number in the center of the bounding box
        std::string numberText = std::to_string(i + 1);
        cv::Size numberSize = cv::getTextSize(numberText, cv::FONT_HERSHEY_SIMPLEX, 2.0, 4, &baseline);
        cv::Point numberPos(det.box.x + det.box.width/2 - numberSize.width/2, 
                           det.box.y + det.box.height/2 + numberSize.height/2);
        
        // Draw number with background circle
        cv::circle(frame, cv::Point(det.box.x + det.box.width/2, det.box.y + det.box.height/2), 
                  numberSize.width + 10, cv::Scalar(0, 0, 0), -1);
        cv::circle(frame, cv::Point(det.box.x + det.box.width/2, det.box.y + det.box.height/2), 
                  numberSize.width + 10, cv::Scalar(255, 255, 255), 3);
        cv::putText(frame, numberText, numberPos, cv::FONT_HERSHEY_SIMPLEX, 2.0, cv::Scalar(255, 255, 255), 4);
    }
    
    // Add instruction text at the bottom
    if (!validDetections.empty()) {
        std::string instructionText = "MANUAL SELECTION MODE - Check console for selection prompt";
        int baseline = 0;
        cv::Size instructionSize = cv::getTextSize(instructionText, cv::FONT_HERSHEY_SIMPLEX, 0.8, 2, &baseline);
        cv::rectangle(frame, 
                     cv::Point(frame.cols/2 - instructionSize.width/2 - 10, frame.rows - 40),
                     cv::Point(frame.cols/2 + instructionSize.width/2 + 10, frame.rows - 5),
                     cv::Scalar(0, 0, 0), -1);
        cv::putText(frame, instructionText, 
                   cv::Point(frame.cols/2 - instructionSize.width/2, frame.rows - 15),
                   cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
    }
}

void Visualizer::visualizeTracking(cv::Mat& frame, bool isTracking, const cv::Rect& trackedBox, 
                                   int trackedClassId, const std::vector<std::string>& classNames,
                                   const std::vector<cv::Point>& trackingPath,
                                   const ControlOutputs* controlOutputs,
                                   uint32_t flightMode,
                                   bool mavlinkConnected,
                                   float centeringRadius,
                                   float yawDeadZoneWidth) {
    if (frame.empty()) return;

    // Draw center point of camera frame
    cv::Point frameCenter(frame.cols / 2.0f, frame.rows / 2.0f);
    cv::circle(frame, frameCenter, 5, cv::Scalar(255, 255, 255), -1); // White center point
    cv::circle(frame, frameCenter, 8, cv::Scalar(0, 0, 0), 2); // Black border
    
    if (isTracking) {
        cv::rectangle(frame, trackedBox, cv::Scalar(0, 255, 0), 2);
        
        // Draw center point of tracked object bounding box
        cv::Point objectCenter(trackedBox.x + trackedBox.width / 2, trackedBox.y + trackedBox.height / 2);
        cv::circle(frame, objectCenter, 5, cv::Scalar(0, 255, 0), -1); // Green center point
        cv::circle(frame, objectCenter, 8, cv::Scalar(0, 0, 0), 2); // Black border
        
        // Draw centering tolerance zone - two horizontal lines around tracked object center
        int halfCenteringRadius = static_cast<int>(centeringRadius / 2.0f);
        int topBoundaryY = std::max(0, objectCenter.y - halfCenteringRadius);
        int bottomBoundaryY = std::min(frame.rows - 1, objectCenter.y + halfCenteringRadius);
        // Draw horizontal lines showing centering tolerance boundaries around tracked object
        cv::line(frame, cv::Point(0, topBoundaryY), cv::Point(frame.cols - 1, topBoundaryY), 
                 cv::Scalar(255, 0, 255), 2); // Magenta top boundary
        cv::line(frame, cv::Point(0, bottomBoundaryY), cv::Point(frame.cols - 1, bottomBoundaryY), 
                 cv::Scalar(255, 0, 255), 2); // Magenta bottom boundary
                 
        // Draw tolerance circle around object center using the configured radius
        // cv::circle(frame, objectCenter, static_cast<int>(centeringRadius), cv::Scalar(255, 0, 255), 
        // Magenta circle
        
        // Draw yaw dead zone - two vertical lines from camera frame center
        int halfDeadZone = static_cast<int>(yawDeadZoneWidth / 2.0f);
        int yawLeftBoundaryX = std::max(0, frameCenter.x - halfDeadZone);
        int yawRightBoundaryX = std::min(frame.cols - 1, frameCenter.x + halfDeadZone);
        
        // Draw vertical lines showing yaw dead zone boundaries
        cv::line(frame, cv::Point(yawLeftBoundaryX, 0), cv::Point(yawLeftBoundaryX, frame.rows - 1), 
                 cv::Scalar(255, 165, 0), 2); // Orange left boundary
        cv::line(frame, cv::Point(yawRightBoundaryX, 0), cv::Point(yawRightBoundaryX, frame.rows - 1), 
                 cv::Scalar(255, 165, 0), 2); // Orange right boundary
        
        // Draw line between frame center and object center
        cv::line(frame, frameCenter, objectCenter, cv::Scalar(255, 255, 0), 2); // Yellow line
        
        std::string className = (trackedClassId >= 0 && trackedClassId < static_cast<int>(classNames.size())) ?
                               classNames[trackedClassId] : "Unknown";
        std::string label = "Tracking: " + className;
        cv::putText(frame, label, cv::Point(trackedBox.x, trackedBox.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
        
        // Create black background bar at the top for status information
        int barHeight = 40;
        cv::rectangle(frame, cv::Point(0, 0), cv::Point(frame.cols, barHeight), cv::Scalar(0, 0, 0), -1);
        
        // Build status text string with all information in one line
        std::string statusLine;
        int xPos = 10;
        float fontSize = 0.7;
        int fontThickness = 2;
        
        // MAVLink status
        if (mavlinkConnected) {
            statusLine = "MAVLink: CONNECTED";
            cv::putText(frame, statusLine, cv::Point(xPos, 28),
                       cv::FONT_HERSHEY_SIMPLEX, fontSize, cv::Scalar(0, 255, 0), fontThickness);
            xPos += cv::getTextSize(statusLine, cv::FONT_HERSHEY_SIMPLEX, fontSize, fontThickness, nullptr).width + 20;
            
            // Flight mode
            std::string flightModeText = "Mode: " + getFlightModeName(flightMode);
            cv::Scalar flightModeColor = (flightMode == 2) ? cv::Scalar(0, 255, 0) : cv::Scalar(200, 200, 200);
            cv::putText(frame, flightModeText, cv::Point(xPos, 28),
                       cv::FONT_HERSHEY_SIMPLEX, fontSize, flightModeColor, fontThickness);
            xPos += cv::getTextSize(flightModeText, cv::FONT_HERSHEY_SIMPLEX, fontSize, fontThickness, nullptr).width + 20;
        } else {
            statusLine = "MAVLink: DISCONNECTED";
            cv::putText(frame, statusLine, cv::Point(xPos, 28),
                       cv::FONT_HERSHEY_SIMPLEX, fontSize, cv::Scalar(0, 0, 255), fontThickness);
            xPos += cv::getTextSize(statusLine, cv::FONT_HERSHEY_SIMPLEX, fontSize, fontThickness, nullptr).width + 20;
        }
        
        // Display control outputs if available
        if (controlOutputs) {
            // RC command status
            std::string rcStatusText = controlOutputs->rc_commands_sent ? "RC: ACTIVE" : "RC: SUPPRESSED";
            cv::Scalar rcStatusColor = controlOutputs->rc_commands_sent ? cv::Scalar(0, 255, 255) : cv::Scalar(0, 255, 0);
            cv::putText(frame, rcStatusText, cv::Point(xPos, 28),
                       cv::FONT_HERSHEY_SIMPLEX, fontSize, rcStatusColor, fontThickness);
            xPos += cv::getTextSize(rcStatusText, cv::FONT_HERSHEY_SIMPLEX, fontSize, fontThickness, nullptr).width + 20;
            
            // Control outputs
            std::string controlText = "Roll: " + std::to_string(controlOutputs->roll_output) + 
                                    " Pitch: " + std::to_string(controlOutputs->pitch_output) +
                                    " Yaw: " + std::to_string(controlOutputs->yaw_output);
            cv::putText(frame, controlText, cv::Point(xPos, 28),
                       cv::FONT_HERSHEY_SIMPLEX, fontSize, cv::Scalar(255, 255, 0), fontThickness);
            xPos += cv::getTextSize(controlText, cv::FONT_HERSHEY_SIMPLEX, fontSize, fontThickness, nullptr).width + 20;
            
            // Filtered errors
            std::string errorText = "Err X: " + std::to_string(static_cast<int>(controlOutputs->error.x)) + 
                                  " Y: " + std::to_string(static_cast<int>(controlOutputs->error.y));
            cv::putText(frame, errorText, cv::Point(xPos, 28),
                       cv::FONT_HERSHEY_SIMPLEX, fontSize, cv::Scalar(255, 255, 0), fontThickness);
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
        LOG_WARN("One or both frames are empty for combined view");
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
    // Calculate position for "DETECTION" label on the right side of the left panel
    int detectionLabelWidth = cv::getTextSize("DETECTION", cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, nullptr).width;
    cv::putText(combinedFrame, "DETECTION", cv::Point(detectionWidth - detectionLabelWidth - 10, 30),
               cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
    
    // Calculate position for "TRACKING" label on the right side of the right panel
    int trackingLabelWidth = cv::getTextSize("TRACKING", cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, nullptr).width;
    cv::putText(combinedFrame, "TRACKING", cv::Point(combinedFrame.cols - trackingLabelWidth - 10, 30),
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
