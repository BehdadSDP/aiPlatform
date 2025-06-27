#ifndef VIDEO_HANDLER_H
#define VIDEO_HANDLER_H

#include <opencv4/opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include "frame_buffer_manager.h"
#include "control_unit.h"

class VideoException : public std::runtime_error {
public:
    explicit VideoException(const std::string& message)
        : std::runtime_error("Video Error: " + message) {}
};

class VideoHandler {
public:
    explicit VideoHandler(ControlUnit& controlUnit)
        : controlUnit_(controlUnit), isStreaming_(false), fps_(0.0), frameCount_(0) {}

    ~VideoHandler() { cleanup(); }

    void initialize(const std::string& videoPath);
    void startStreaming();
    void stopStreaming();
    void cleanup();
    
    // Get video properties
    double getFrameRate() const { return fps_; }
    int getTotalFrames() const { return totalFrames_; }
    cv::Size getFrameSize() const { return frameSize_; }
    
    bool isOpened() const { return cap_.isOpened(); }

private:
    void streamingLoop();
    
    cv::VideoCapture cap_;
    ControlUnit& controlUnit_;
    
    std::atomic<bool> isStreaming_;
    std::thread streamingThread_;
    
    // Video properties
    double fps_;
    int totalFrames_;
    cv::Size frameSize_;
    
    // Frame tracking
    unsigned int frameCount_;
    std::chrono::steady_clock::time_point startTime_;
};

#endif // VIDEO_HANDLER_H 