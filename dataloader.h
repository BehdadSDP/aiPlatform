// dataloader.h
#ifndef DATALOADER_H
#define DATALOADER_H

#include <vector>
#include <opencv2/opencv.hpp>
#include <mutex>
#include <condition_variable>

class DataLoader {
public:
    DataLoader(size_t batchSize = 1) : batchSize_(batchSize), ready_(false) {}

    // Add a frame to the buffer
    void addFrame(const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (frames_.size() < batchSize_) {
            frames_.push_back(frame.clone());
            if (frames_.size() == batchSize_) {
                ready_ = true;
                condVar_.notify_one();
            }
        }
    }

    // Get a batch of frames when ready
    bool getBatch(std::vector<cv::Mat>& batch) {
        std::unique_lock<std::mutex> lock(mutex_);
        condVar_.wait(lock, [this] { return ready_ || stopped_; });

        if (stopped_ && frames_.empty()) {
            return false; // No more data
        }

        if (ready_) {
            batch = frames_;
            frames_.clear();
            ready_ = false;
            return true;
        }
        return false;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        condVar_.notify_all();
    }

private:
    std::vector<cv::Mat> frames_;
    size_t batchSize_;
    bool ready_;
    bool stopped_ = false;
    std::mutex mutex_;
    std::condition_variable condVar_;
};

#endif // DATALOADER_H
