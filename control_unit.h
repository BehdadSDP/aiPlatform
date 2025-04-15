#pragma once
#include "shared_data.h"
#include <opencv2/opencv.hpp>
#include <atomic>
#include <condition_variable>
#include <chrono>

class ControlUnit {
public:
    ControlUnit() : detectionMode_(0), detectionInterval_(500), trackingInterval_(20) {}
    
    // Detection mode management
    void setDetectionMode(int mode);
    int getDetectionMode() const { return detectionMode_; }
    
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
    void setDetection(const cv::Rect& box, const cv::Mat& frame, uint64_t frameSeq, int classId);
    void clearDetection();
    void getDetectionData(cv::Rect& box, cv::Mat& frame, int& classId) const;
    void markDetectionAsProcessed();
    
private:
    // Detection mode
    int detectionMode_{0}; // 0: time-based, 1: tracker-initialization-based
    
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
    mutable std::mutex detectionMutex_;
};
