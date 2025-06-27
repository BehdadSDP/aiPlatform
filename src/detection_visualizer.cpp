#include "include/detection_visualizer.h"
#include "include/traffic_intensity_manager.h"

DetectionVisualizer::DetectionVisualizer() {}

void DetectionVisualizer::visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, 
                                             const std::vector<std::string>& classNames) {
    if (frame.empty()) return;
    
    // Skip visualization if no detections (optimization for empty frames)
    if (detections.empty()) {
        return;  // Don't display here, let detection manager handle it
    }

    // Draw directly on frame to avoid cloning overhead in detection-only mode
    cv::Mat& displayFrame = frame;

    for (const auto& det : detections) {
        if (det.confidence > 0.15f) {
            // Get class name
            std::string className = (det.classId >= 0 && det.classId < static_cast<int>(classNames.size())) ?
                                   classNames[det.classId] : "Unknown";
            
            // Get color and status text based on class
            cv::Scalar boxColor = getClassColor(className);
            std::string statusText = getStatusText(className);
            
            // Draw box with appropriate color (changed thickness from 3 to 2)
            cv::rectangle(displayFrame, det.box, boxColor, 1);

            // Create label with class name, confidence, and status
            std::string label = className + ": " + std::to_string(int(det.confidence * 100)) + "%" + statusText;

            // Add text with background
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
            cv::rectangle(displayFrame,
                         cv::Point(det.box.x, det.box.y - textSize.height - 10),
                         cv::Point(det.box.x + textSize.width, det.box.y),
                         boxColor, -1);

            cv::putText(displayFrame, label,
                       cv::Point(det.box.x, det.box.y - 5),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        }
    }
    
    // Don't display the frame here - let detection manager handle it after adding hazard zones
}

// New overloaded method that considers traffic intensity polygons
void DetectionVisualizer::visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, 
                                             const std::vector<std::string>& classNames,
                                             const TrafficIntensityManager& trafficManager) {
    if (frame.empty()) return;
    
    // Skip visualization if no detections (optimization for empty frames)
    if (detections.empty()) {
        return;  // Don't display here, let detection manager handle it
    }

    // Draw directly on frame to avoid cloning overhead in detection-only mode
    cv::Mat& displayFrame = frame;

    for (const auto& det : detections) {
        if (det.confidence > 0.15f) {
            // If traffic intensity is enabled, only show detections inside traffic polygons
            if (trafficManager.isEnabled() && trafficManager.hasActivePolygons()) {
                if (!isDetectionInTrafficPolygons(det, trafficManager)) {
                    continue; // Skip this detection if it's not in any active traffic polygon
                }
            }
            
            // Get class name
            std::string className = (det.classId >= 0 && det.classId < static_cast<int>(classNames.size())) ?
                                   classNames[det.classId] : "Unknown";
            
            // Get color and status text based on class
            cv::Scalar boxColor = getClassColor(className);
            std::string statusText = getStatusText(className);
            
            // Draw box with appropriate color
            cv::rectangle(displayFrame, det.box, boxColor, 1);

            // Create label with class name, confidence, and status
            std::string label = className + ": " + std::to_string(int(det.confidence * 100)) + "%" + statusText;

            // Add text with background
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
            cv::rectangle(displayFrame,
                         cv::Point(det.box.x, det.box.y - textSize.height - 10),
                         cv::Point(det.box.x + textSize.width, det.box.y),
                         boxColor, -1);

            cv::putText(displayFrame, label,
                       cv::Point(det.box.x, det.box.y - 5),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        }
    }
    
    // Don't display the frame here - let detection manager handle it after adding hazard zones
}

void DetectionVisualizer::setupWindow() {
    if (!windowCreated_) {
        cv::namedWindow("Detection View", cv::WINDOW_NORMAL);
        cv::resizeWindow("Detection View", 800, 600);
        windowCreated_ = true;
    }
}

cv::Scalar DetectionVisualizer::getClassColor(const std::string& className) {
    if (className == "helmet" || className == "hardhat") {
        return cv::Scalar(0, 255, 0); // Green for safe
    } else if (className == "no-helmet" || className == "head") {
        return cv::Scalar(0, 0, 255); // Red for unsafe
    } else if (className == "person") {
        return cv::Scalar(255, 165, 0); // Orange for person
    } else if (className == "vehicle") {
        return cv::Scalar(255, 0, 0); // Blue for vehicles
    } else if (className == "car") {
        return cv::Scalar(255, 0, 0); // Blue for cars
    } else if (className == "truck") {
        return cv::Scalar(0, 100, 255); // Orange-red for trucks
    } else if (className == "bus") {
        return cv::Scalar(255, 0, 255); // Magenta for buses
    } else if (className == "motorcycle") {
        return cv::Scalar(128, 0, 255); // Purple for motorcycles
    } else if (className == "bicycle") {
        return cv::Scalar(0, 255, 128); // Light green for bicycles
    } else {
        return cv::Scalar(0, 255, 255); // Yellow for other classes
    }
}

std::string DetectionVisualizer::getStatusText(const std::string& className) {
    if (className == "helmet" || className == "hardhat") {
        return " ✓ SAFE";
    } else if (className == "no-helmet" || className == "head") {
        return " ⚠ VIOLATION";
    } else if (className == "person") {
        return " - Person";
    } else if (className == "vehicle" || className == "car" || className == "truck" || 
               className == "bus" || className == "motorcycle" || className == "bicycle") {
        return " - Vehicle";
    } else {
        return "";
    }
}

bool DetectionVisualizer::isDetectionInTrafficPolygons(const model::Detection& detection, 
                                                      const TrafficIntensityManager& trafficManager) {
    // Get the center point of the detection bounding box
    cv::Point detectionCenter(detection.box.x + detection.box.width / 2,
                             detection.box.y + detection.box.height / 2);
    
    // Use the new method to check if the detection center is in any active polygon
    return trafficManager.isPointInActivePolygon(detectionCenter);
} 
