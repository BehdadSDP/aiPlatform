#include "include/detection_visualizer.h"

DetectionVisualizer::DetectionVisualizer() {}

void DetectionVisualizer::visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, 
                                             const std::vector<std::string>& classNames) {
    if (frame.empty() || detections.empty()) return;

    cv::Mat displayFrame = frame.clone();

    for (const auto& det : detections) {
        if (det.confidence > 0.15f) {
            // Get class name
            std::string className = (det.classId >= 0 && det.classId < static_cast<int>(classNames.size())) ?
                                   classNames[det.classId] : "Unknown";
            
            // Get color and status text based on class
            cv::Scalar boxColor = getClassColor(className);
            std::string statusText = getStatusText(className);
            
            // Draw box with appropriate color
            cv::rectangle(displayFrame, det.box, boxColor, 3);

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

    setupWindow();
    cv::imshow("Detection View", displayFrame);
    cv::waitKey(1);
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
    } else {
        return "";
    }
} 