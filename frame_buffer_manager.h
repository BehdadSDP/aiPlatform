#ifndef FRAME_BUFFER_MANAGER_H
#define FRAME_BUFFER_MANAGER_H

#include <opencv4/opencv2/opencv.hpp>
#include <array>
#include <mutex>
#include <condition_variable>

struct FrameData {
    cv::Mat image;
    int64_t timestamp;
    unsigned int sequence;
    std::string format;
    cv::Size size;
    double fps;

    FrameData() : timestamp(0), sequence(0), format(""), size(0, 0), fps(0.0) {}
};

class FrameBufferManager {
public:
    static FrameBufferManager& getInstance() {
        static FrameBufferManager instance;
        return instance;
    }

    void addFrame(const FrameData& frameData) {
        std::lock_guard<std::mutex> lock(mutex_);
        frames_[tail_] = frameData; // Copy the entire FrameData struct
        tail_ = (tail_ + 1) % Buffersize;
        if (size_ < Buffersize) size_++;
        else head_ = (head_ + 1) % Buffersize;
        condVar_.notify_all();
    }

    bool getLatestFrame(FrameData& frameData) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ == 0) return false;
        size_t latestIndex = (tail_ == 0) ? (Buffersize - 1) : (tail_ - 1);
        frameData = frames_[latestIndex]; // Copy the entire FrameData
        frameData.image = frames_[latestIndex].image.clone(); // Deep copy the image
        return true;
    }

//    bool dequeueFrame(FrameData& frameData) {
//        std::lock_guard<std::mutex> lock(mutex_);
//        if (size_ == 0) return false;
//        frameData = frames_[head_];
//        frameData.image = frames_[head_].image.clone(); // Deep copy the image
//        head_ = (head_ + 1) % Buffersize;
//        size_--;
//        condVar_.notify_all();
//        return true;
//    }

    void getAllFrames(std::vector<FrameData>& frames) {
        std::lock_guard<std::mutex> lock(mutex_);
        frames.clear();
        if (size_ == 0) return;
        size_t index = head_;
        for (size_t i = 0; i < size_; ++i) {
            FrameData fd = frames_[index];
            fd.image = frames_[index].image.clone(); // Deep copy each image
            frames.push_back(fd);
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
    std::array<FrameData, Buffersize> frames_;
    size_t head_;
    size_t tail_;
    size_t size_;
    mutable std::mutex mutex_;
    std::condition_variable condVar_;
};

#endif // FRAME_BUFFER_MANAGER_H
