#pragma once
#include <opencv2/opencv.hpp>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <vector>
#include "tracker_interface.h"

class ControlUnit {
public:
    ControlUnit() : detectionMode_(0), operationMode_(0), detectionInterval_(500), trackingInterval_(20) {}
    
    // Detection mode management
    void setDetectionMode(int mode);
    int getDetectionMode() const { return detectionMode_; }
    
    // Operation mode management
    void setOperationMode(int mode);
    int getOperationMode() const { return operationMode_; }
    bool isDetectionOnly() const { return operationMode_ == 1; }
    
    // Time-based interval settings
    void setDetectionInterval(int milliseconds);
    void setTrackingInterval(int milliseconds);
    
    // Thread synchronization methods
    void notifyNewFrame();
    bool waitForDetectionTurn(int timeoutMs = 100);
    bool waitForTrackingTurn(int timeoutMs = 50);
    
    // Detection data management
    bool hasValidDetection() const;
    bool hasNewDetection() const;
    bool hasTrackerFailed() const;
    void setTrackerFailed(bool failed);
    void setTrackerFailed(bool failed, const cv::Mat& frame, const cv::Rect& box);
    void getFailureData(cv::Mat& frame, cv::Rect& box) const;
    void setDetection(const cv::Rect& box, const cv::Mat& frame, uint64_t frameSeq, int classId);
    void clearDetection();
    void getDetectionData(cv::Rect& box, cv::Mat& frame, int& classId) const;
    void markDetectionAsProcessed();

    // Tracking state management
    void setIsTracking(bool isTracking);
    bool isTracking() const;
    
private:
    // Detection mode
    int detectionMode_{0}; // 0: time-based, 1: tracker-initialization-based
    
    // Operation mode
    int operationMode_{0}; // 0: detection+tracking, 1: detection only
    
    // Thread synchronization
    std::mutex syncMutex_;
    std::condition_variable detectionCV_;
    std::condition_variable trackingCV_;
    std::chrono::steady_clock::time_point lastDetectionTime_;
    std::chrono::steady_clock::time_point lastTrackingTime_;
    int detectionInterval_; // milliseconds
    int trackingInterval_;  // milliseconds
    
    // Detection data
    struct DetectionData {
        cv::Rect box;
        bool valid{false};
        uint64_t frameSeq{0};
        cv::Mat frame;
        bool newDetection{false};
        int classId{-1};
    };
    
    DetectionData detection_;
    bool trackerFailed_{false};
    cv::Mat lastFailedFrame_;
    cv::Rect lastFailedBox_;
    mutable std::mutex detectionMutex_;

    // Tracking state
    bool isTracking_{false};
};
