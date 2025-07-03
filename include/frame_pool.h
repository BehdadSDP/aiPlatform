#ifndef FRAME_POOL_H
#define FRAME_POOL_H

#include <opencv4/opencv2/opencv.hpp>
#include <queue>
#include <mutex>
#include <memory>

class FramePool {
public:
    static FramePool& getInstance() {
        static FramePool instance;
        return instance;
    }

    // Acquire a frame from the pool
    std::unique_ptr<cv::Mat> acquireFrame() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (availableFrames_.empty()) {
            // Create new frame if pool is empty
            return std::make_unique<cv::Mat>();
        }
        
        auto frame = std::move(availableFrames_.front());
        availableFrames_.pop();
        
        // Clear any existing data
        frame->release();
        return frame;
    }
    
    // Release a frame back to the pool
    void releaseFrame(std::unique_ptr<cv::Mat> frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (frame) {
            // Clear the frame data to free memory
            frame->release();
            
            // Limit pool size to prevent memory accumulation
            if (availableFrames_.size() < MAX_POOL_SIZE) {
                availableFrames_.push(std::move(frame));
            }
        }
    }
    
    // Get pool statistics
    size_t getPoolSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return availableFrames_.size();
    }
    
    // Clear the entire pool
    void clearPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!availableFrames_.empty()) {
            availableFrames_.pop();
        }
    }

private:
    FramePool() = default;
    FramePool(const FramePool&) = delete;
    FramePool& operator=(const FramePool&) = delete;
    
    static constexpr size_t MAX_POOL_SIZE = 10; // Limit pool size
    
    std::queue<std::unique_ptr<cv::Mat>> availableFrames_;
    mutable std::mutex mutex_;
};

#endif // FRAME_POOL_H 