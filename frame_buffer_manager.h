#ifndef FRAME_BUFFER_MANAGER_H
#define FRAME_BUFFER_MANAGER_H

#include <opencv4/opencv2/opencv.hpp>
#include <array>
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
        frames_[tail_] = frame.clone();
        tail_ = (tail_ + 1) % Buffersize;
        if (size_ < Buffersize) size_++;
        else head_ = (head_ + 1) % Buffersize;
        condVar_.notify_all();
    }

    bool getLatestFrame(cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ == 0) return false;
        size_t latestIndex = (tail_ == 0) ? (Buffersize - 1) : (tail_ - 1);
        frame = frames_[latestIndex].clone();
        return true;
    }

    void getAllFrames(std::vector<cv::Mat>& frames) {
        std::lock_guard<std::mutex> lock(mutex_);
        frames.clear();
        if (size_ == 0) return;
        size_t index = head_;
        for (size_t i = 0; i < size_; ++i) {
            frames.push_back(frames_[index].clone());
            index = (index + 1) % Buffersize;
        }
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    void waitForNewFrame() {
        std::unique_lock<std::mutex> lock(mutex_);
        condVar_.wait(lock, [this] { return size_ > 0; });
    }

    void clearFrames() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        tail_ = 0;
        size_ = 0;
        condVar_.notify_all();
    }

private:
    FrameBufferManager() : head_(0), tail_(0), size_(0) {}
    FrameBufferManager(const FrameBufferManager&) = delete;
    FrameBufferManager& operator=(const FrameBufferManager&) = delete;

    static constexpr size_t Buffersize = 500;
    std::array<cv::Mat, Buffersize> frames_;
    size_t head_;
    size_t tail_;
    size_t size_;
    mutable std::mutex mutex_;
    std::condition_variable condVar_;
};

#endif // FRAME_BUFFER_MANAGER_H
