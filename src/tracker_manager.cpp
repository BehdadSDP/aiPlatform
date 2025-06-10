#include "include/tracker_manager.h"
#include "include/frame_buffer_manager.h"
#include <iostream>
#include <chrono>

TrackerManager::TrackerManager(std::unique_ptr<TrackerInterface> tracker) 
    : tracker_(std::move(tracker)) {}

void TrackerManager::runTrackingLoop(std::atomic<bool>& running, ModelManager& modelManager, 
                                    ControlUnit& controlUnit) {
    int mode = controlUnit.getDetectionMode();

    while (running) {
        // Wait for turn to run tracking
        if (!controlUnit.waitForTrackingTurn()) {
            continue;
        }

        // Check for new detection to initialize
        bool shouldInitialize = controlUnit.hasNewDetection();
        cv::Rect yoloBox;
        cv::Mat detectionFrame;
        int classId = -1;

        if (shouldInitialize) {
            controlUnit.getDetectionData(yoloBox, detectionFrame, classId);
            controlUnit.markDetectionAsProcessed();

            if (!detectionFrame.empty()) {
                std::cout << "Detection box: x=" << yoloBox.x << ", y=" << yoloBox.y
                         << ", width=" << yoloBox.width << ", height=" << yoloBox.height << std::endl;
            }
        }

        // Initialize tracker if needed
        if ((shouldInitialize && mode == 0) || (mode == 1 && !isTracking_)) {
            if (!detectionFrame.empty()) {
                initializeTracker(detectionFrame, yoloBox, classId, modelManager.getClassNames());
            }
        }

        // Get frame for update
        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) continue;

        // Update tracker if tracking
        if (isTracking_) {
            updateTracker(frame, controlUnit);
        }

        // Always visualize, even if not tracking
        visualizeTracking(frame);
    }
    cv::destroyAllWindows();
}

void TrackerManager::initializeTracker(const cv::Mat& frame, const cv::Rect& bbox, int classId,
                                      const std::vector<std::string>& classNames) {
    try {
        tracker_->model_initializer(frame, bbox);
        isTracking_ = true;
        lastTrackBox_ = bbox;
        
        std::string className = (classId >= 0 && classId < static_cast<int>(classNames.size())) ?
                              classNames[classId] : "Unknown";
        std::cout << "Tracker: Initialized, Class: " << className << std::endl;

        // Visualize initial detection box
        visualizeTracking(frame);
    } catch (const std::exception& e) {
        std::cerr << "Tracker initialization failed: " << e.what() << std::endl;
        isTracking_ = false;
    }
}

void TrackerManager::updateTracker(const cv::Mat& frame, ControlUnit& controlUnit) {
    try {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        lastTrackBox_ = tracker_->update(frame);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        float currentFps = 1000000.0f / duration;
        
        bool trackerValid = lastTrackBox_.width > 0 && lastTrackBox_.height > 0 && tracker_->isInitialized();
        if (!trackerValid) {
            isTracking_ = false;
            trackingPath_.clear(); // Clear path when tracking is lost
            controlUnit.setTrackerFailed(true);
            std::cout << "Tracker: Tracking lost, last confidence: " << tracker_->getLastConfidence() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Tracker update failed: " << e.what() << std::endl;
        isTracking_ = false;
        trackingPath_.clear();
        controlUnit.setTrackerFailed(true);
    }
}

void TrackerManager::visualizeTracking(const cv::Mat& frame) {
    if (frame.empty()) return;

    cv::Mat displayFrame = frame.clone();
    
    if (isTracking_ && tracker_->isInitialized()) {
        // Calculate center point of current tracking box
        cv::Point currentCenter(lastTrackBox_.x + lastTrackBox_.width / 2, 
                               lastTrackBox_.y + lastTrackBox_.height / 2);
        
        // Add current center to tracking path
        trackingPath_.push_back(currentCenter);
        
        // Limit path size to prevent memory growth
        if (trackingPath_.size() > MAX_PATH_POINTS) {
            trackingPath_.erase(trackingPath_.begin());
        }
        
        // Draw tracking path with gradually fading lines
        if (trackingPath_.size() > 1) {
            for (size_t i = 1; i < trackingPath_.size(); ++i) {
                float alpha = static_cast<float>(i) / trackingPath_.size();
                int thickness = static_cast<int>(1 + alpha * 3);
                cv::Scalar fadeColor = pathColor_ * alpha;
                cv::line(displayFrame, trackingPath_[i-1], trackingPath_[i], fadeColor, thickness);
            }
            
            // Draw path points as small circles
            for (size_t i = 0; i < trackingPath_.size(); ++i) {
                float alpha = static_cast<float>(i) / trackingPath_.size();
                int radius = static_cast<int>(2 + alpha * 3);
                cv::Scalar pointColor = pathColor_ * alpha;
                cv::circle(displayFrame, trackingPath_[i], radius, pointColor, -1);
            }
        }
        
        // Draw current tracking box
        cv::rectangle(displayFrame, lastTrackBox_, cv::Scalar(0, 0, 255), 3);

        // Add text with path info
        std::string label = "Tracking (Path: " + std::to_string(trackingPath_.size()) + " points)";
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
        cv::rectangle(displayFrame,
                     cv::Point(lastTrackBox_.x, lastTrackBox_.y - textSize.height - 10),
                     cv::Point(lastTrackBox_.x + textSize.width, lastTrackBox_.y),
                     cv::Scalar(0, 0, 255), -1);

        cv::putText(displayFrame, label,
                   cv::Point(lastTrackBox_.x, lastTrackBox_.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
                   
        // Draw current position marker
        cv::circle(displayFrame, currentCenter, 6, cv::Scalar(0, 255, 255), 2);
        cv::circle(displayFrame, currentCenter, 3, cv::Scalar(255, 255, 255), -1);
    } else {
        // Clear path when not tracking
        if (!isTracking_) {
            trackingPath_.clear();
        }
    }

    // Make sure window is created before showing image
    static bool windowCreated = false;
    if (!windowCreated) {
        cv::namedWindow("Tracker View", cv::WINDOW_NORMAL);
        cv::resizeWindow("Tracker View", 800, 600);
        windowCreated = true;
    }
    cv::imshow("Tracker View", displayFrame);
    cv::waitKey(1);
} 