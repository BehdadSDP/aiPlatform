#include "include/detection/manual_detector.h"
#include "include/logger.h"
#include <algorithm>

ManualDetector::ManualDetector() : selectionComplete_(false) {
}

void ManualDetector::mouseCallback(int event, int x, int y, int flags, void* userdata) {
    ManualDetector* detector = static_cast<ManualDetector*>(userdata);
    if (detector) {
        detector->handleMouseEvent(event, x, y);
    }
}

void ManualDetector::handleMouseEvent(int event, int x, int y) {
    if (event == cv::EVENT_LBUTTONDOWN) {
        std::lock_guard<std::mutex> lock(selectionMutex_);
        
        // If selection was complete, reset and start new selection
        if (selectionComplete_.load()) {
            selectedPoints_.clear();
            selectionComplete_.store(false);
            LOG_INFO("Manual selection: Starting new selection");
        }
        
        if (selectedPoints_.size() < 2) {
            selectedPoints_.push_back(cv::Point(x, y));
            LOG_INFO("Manual selection: Point {} selected at ({}, {})", selectedPoints_.size(), x, y);
            
            if (selectedPoints_.size() == 2) {
                selectionComplete_.store(true);
                LOG_INFO("Manual selection: Bounding box complete!");
            }
            
            // Update visualization
            if (!displayFrame_.empty()) {
                cv::Mat tempFrame = displayFrame_.clone();
                drawSelection(tempFrame);
                // Don't call imshow here - let the main visualization handle it
            }
        }
    } else if (event == cv::EVENT_RBUTTONDOWN) {
        // Right click to reset selection
        reset();
        LOG_INFO("Manual selection: Reset (right-click)");
    }
}

void ManualDetector::drawSelection(cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(selectionMutex_);
    
    if (selectedPoints_.empty()) return;
    
    // Draw first point
    cv::circle(frame, selectedPoints_[0], 5, cv::Scalar(0, 255, 0), -1);
    cv::circle(frame, selectedPoints_[0], 7, cv::Scalar(255, 255, 255), 2);
    
    // Draw second point and rectangle if both points selected
    if (selectedPoints_.size() == 2) {
        cv::circle(frame, selectedPoints_[1], 5, cv::Scalar(0, 255, 0), -1);
        cv::circle(frame, selectedPoints_[1], 7, cv::Scalar(255, 255, 255), 2);
        
        // Draw rectangle
        cv::Rect box(selectedPoints_[0], selectedPoints_[1]);
        cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
        
        // Add instruction text - different message based on state
        if (selectionComplete_.load()) {
            std::string text = "ROI Selected - Tracking active (Click to reselect)";
            cv::putText(frame, text, 
                       cv::Point(10, 30),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
        } else {
            std::string text = "ROI Selected - Initializing tracker...";
            cv::putText(frame, text, 
                       cv::Point(10, 30),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 165, 0), 2);
        }
    } else {
        // Show instruction for first point - smaller text at top
        std::string text = "Click second point (Right-click to reset)";
        cv::putText(frame, text, 
                   cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 165, 255), 2);
    }
}

std::vector<model::Detection> ManualDetector::detect(const cv::Mat& frame, const std::string& windowName) {
    if (frame.empty()) {
        return {};
    }
    
    windowName_ = windowName;
    
    // Store a REFERENCE to the frame, not a clone
    // We'll draw directly on the input frame so visualization sees our marks
    displayFrame_ = frame.clone(); // Still keep a copy for mouse callback reference
    
    // Draw selection on the ACTUAL input frame (non-const version)
    cv::Mat& frameRef = const_cast<cv::Mat&>(frame);
    drawSelection(frameRef);
    
    // Add initial instruction if no points selected
    if (selectedPoints_.empty()) {
        std::string text = "MANUAL MODE - Click 2 points to select target";
        cv::putText(frameRef, text, 
                   cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 165, 255), 2);
    }
    
    // Set mouse callback for the window (only once)
    static bool callbackSet = false;
    if (!callbackSet) {
        cv::setMouseCallback(windowName_, ManualDetector::mouseCallback, this);
        callbackSet = true;
        LOG_INFO("Manual detector: Mouse callback registered for window '{}'", windowName_);
    }
    
    // Check if selection is complete
    if (selectionComplete_.load()) {
        std::vector<model::Detection> detections;
        detections.push_back(createDetectionFromPoints());
        
        // Don't reset here - let the next click reset it
        // This allows the detection to persist until tracker initializes
        
        return detections;
    }
    
    return {};  // No detection yet
}

model::Detection ManualDetector::createDetectionFromPoints() {
    std::lock_guard<std::mutex> lock(selectionMutex_);
    
    model::Detection detection;
    
    if (selectedPoints_.size() == 2) {
        // Create rectangle from two points
        cv::Point topLeft(std::min(selectedPoints_[0].x, selectedPoints_[1].x),
                         std::min(selectedPoints_[0].y, selectedPoints_[1].y));
        cv::Point bottomRight(std::max(selectedPoints_[0].x, selectedPoints_[1].x),
                             std::max(selectedPoints_[0].y, selectedPoints_[1].y));
        
        detection.box = cv::Rect(topLeft, bottomRight);
        detection.confidence = 1.0f;  // Manual selection is 100% confident
        detection.classId = 0;  // Generic manual selection class
    }
    
    return detection;
}

bool ManualDetector::hasSelection() const {
    return selectionComplete_.load();
}

void ManualDetector::reset() {
    std::lock_guard<std::mutex> lock(selectionMutex_);
    selectedPoints_.clear();
    selectionComplete_.store(false);
}

bool ManualDetector::getSelectedBox(cv::Rect& box) const {
    std::lock_guard<std::mutex> lock(selectionMutex_);
    
    if (selectedPoints_.size() == 2) {
        cv::Point topLeft(std::min(selectedPoints_[0].x, selectedPoints_[1].x),
                         std::min(selectedPoints_[0].y, selectedPoints_[1].y));
        cv::Point bottomRight(std::max(selectedPoints_[0].x, selectedPoints_[1].x),
                             std::max(selectedPoints_[0].y, selectedPoints_[1].y));
        
        box = cv::Rect(topLeft, bottomRight);
        return box.area() > 0;
    }
    
    return false;
}
