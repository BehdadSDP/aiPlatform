#ifndef FRAME_BUFFER_MANAGER_H
#define FRAME_BUFFER_MANAGER_H

#include <opencv4/opencv2/opencv.hpp>
#include <vector>
#include <mutex>
#include <condition_variable>

class FrameBufferManager {
public:
    static FrameBufferManager& getInstance() {
        static FrameBufferManager instance;
        return instance;
    }

    void addFrame(const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (frames_.size() < maxFrames_) {
            frames_.push_back(frame.clone());
            condVar_.notify_all(); // Notify waiting threads (e.g., YOLO)
        }
    }

    bool getLatestFrame(cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (frames_.empty()) return false;
        frame = frames_.back().clone(); // Return the most recent frame
        return true;
    }

    void getAllFrames(std::vector<cv::Mat>& frames) {
        std::lock_guard<std::mutex> lock(mutex_);
        frames = frames_; // Copy all frames
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_.size();
    }

    void waitForNewFrame() {
        std::unique_lock<std::mutex> lock(mutex_);
        condVar_.wait(lock, [this] { return !frames_.empty(); });
    }

    void clearFrames() {
        std::lock_guard<std::mutex> lock(mutex_);
        frames_.clear();
        condVar_.notify_all();
    }

private:
    FrameBufferManager() : maxFrames_(500) {}
    FrameBufferManager(const FrameBufferManager&) = delete;
    FrameBufferManager& operator=(const FrameBufferManager&) = delete;

    std::vector<cv::Mat> frames_;
    const size_t maxFrames_;
    mutable std::mutex mutex_;
    std::condition_variable condVar_;
};

#endif // FRAME_BUFFER_MANAGER_H
