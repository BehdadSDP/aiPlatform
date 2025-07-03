#include "include/video_handler.h"
#include <iostream>
#include <iomanip>

void VideoHandler::initialize(const std::string& videoPath) {
    if (videoPath.empty()) {
        throw VideoException("Video path cannot be empty");
    }
    
    cap_.open(videoPath);
    if (!cap_.isOpened()) {
        throw VideoException("Failed to open video file: " + videoPath);
    }
    
    // Get video properties
    fps_ = cap_.get(cv::CAP_PROP_FPS);
    totalFrames_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_COUNT));
    frameSize_.width = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    frameSize_.height = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
    
    std::cout << "Video initialized successfully:" << std::endl;
    std::cout << "  Resolution: " << frameSize_.width << "x" << frameSize_.height << std::endl;
    std::cout << "  FPS: " << fps_ << std::endl;
    std::cout << "  Total frames: " << totalFrames_ << std::endl;
    std::cout << "  Duration: " << (totalFrames_ / fps_) << " seconds" << std::endl;
}

void VideoHandler::startStreaming() {
    if (!cap_.isOpened()) {
        throw VideoException("Video not initialized. Call initialize() first.");
    }
    
    if (isStreaming_) {
        std::cout << "Video streaming already started" << std::endl;
        return;
    }
    
    isStreaming_ = true;
    frameCount_ = 0;
    startTime_ = std::chrono::steady_clock::now();
    
    streamingThread_ = std::thread(&VideoHandler::streamingLoop, this);
    std::cout << "Video streaming started" << std::endl;
}

void VideoHandler::stopStreaming() {
    if (!isStreaming_) {
        return;
    }
    
    isStreaming_ = false;
    
    if (streamingThread_.joinable()) {
        streamingThread_.join();
    }
    
    std::cout << "Video streaming stopped" << std::endl;
}

void VideoHandler::cleanup() {
    stopStreaming();
    
    if (cap_.isOpened()) {
        cap_.release();
    }
    
    frameCount_ = 0;
}

void VideoHandler::streamingLoop() {
    cv::Mat frame;
    const double frameDelay = 1000.0 / fps_; // Delay in milliseconds
    auto lastFrameTime = std::chrono::steady_clock::now();
    
    while (isStreaming_) {
        // Read frame from video
        if (!cap_.read(frame)) {
            // End of video reached, restart from beginning for continuous playback
            cap_.set(cv::CAP_PROP_POS_FRAMES, 0);
            frameCount_ = 0;
            std::cout << "End of video reached, restarting playback..." << std::endl;
            continue;
        }
        
        if (frame.empty()) {
            std::cerr << "Warning: Empty frame read from video" << std::endl;
            continue;
        }
        
        // Create frame data
        FrameData frameData;
        frameData.image = frame.clone();
        frameData.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        frameData.sequence = frameCount_++;
        frameData.format = "BGR";
        frameData.size = cv::Size(frame.cols, frame.rows);
        frameData.fps = fps_;
        
        // ✅ OPTIMIZED: Use move semantics to add frame to buffer manager
        FrameBufferManager::getInstance().addFrame(std::move(frameData));
        
        // Calculate timing for next frame
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastFrameTime).count();
        
        // Maintain video frame rate
        if (elapsed < frameDelay) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(frameDelay - elapsed)));
        }
        
        lastFrameTime = std::chrono::steady_clock::now();
        
        // Optional: Print progress every 100 frames
        if (frameCount_ % 100 == 0) {
            double progress = (static_cast<double>(frameCount_) / totalFrames_) * 100.0;
            std::cout << "Video progress: " << std::fixed << std::setprecision(1) 
                      << progress << "% (Frame " << frameCount_ << "/" << totalFrames_ << ")" << std::endl;
        }
    }
} 