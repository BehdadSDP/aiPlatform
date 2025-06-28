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
    
    // Copy constructor
    FrameData(const FrameData& other) 
        : image(other.image.clone()), timestamp(other.timestamp), 
          sequence(other.sequence), format(other.format), 
          size(other.size), fps(other.fps) {}
    
    // Copy assignment operator
    FrameData& operator=(const FrameData& other) {
        if (this != &other) {
            image = other.image.clone();
            timestamp = other.timestamp;
            sequence = other.sequence;
            format = other.format;
            size = other.size;
            fps = other.fps;
        }
        return *this;
    }
    
    // Move constructor
    FrameData(FrameData&& other) noexcept 
        : image(std::move(other.image)), timestamp(other.timestamp), 
          sequence(other.sequence), format(std::move(other.format)), 
          size(other.size), fps(other.fps) {}
    
    // Move assignment operator
    FrameData& operator=(FrameData&& other) noexcept {
        if (this != &other) {
            image.release();
            image = std::move(other.image);
            timestamp = other.timestamp;
            sequence = other.sequence;
            format = std::move(other.format);
            size = other.size;
            fps = other.fps;
        }
        return *this;
    }
    
    // Destructor
    ~FrameData() {
        image.release();
    }
};

class FrameBufferManager {
public:
    static FrameBufferManager& getInstance() {
        static FrameBufferManager instance;
        return instance;
    }

    void addFrame(const FrameData& frameData) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Explicitly release old frame memory before overwriting
        if (size_ >= Buffersize) {
            frames_[head_].image.release();
        }
        
        frames_[tail_] = frameData; // Copy the entire FrameData struct
        tail_ = (tail_ + 1) % Buffersize;
        if (size_ < Buffersize) size_++;
        else head_ = (head_ + 1) % Buffersize;
        
        // Increment frame counter for automatic cleanup
        frameCounter_++;
        
        // Automatic cleanup every CLEANUP_INTERVAL frames
        if (frameCounter_ >= CLEANUP_INTERVAL) {
            performCleanup();
            frameCounter_ = 0;
        }
        
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
        performCleanup();
    }
    
    // Configuration methods
    void setCleanupInterval(int interval) {
        std::lock_guard<std::mutex> lock(mutex_);
        cleanupInterval_ = interval;
    }
    
    int getCleanupInterval() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cleanupInterval_;
    }

private:
    FrameBufferManager() : head_(0), tail_(0), size_(0), frameCounter_(0), cleanupInterval_(CLEANUP_INTERVAL) {}
    FrameBufferManager(const FrameBufferManager&) = delete;
    FrameBufferManager& operator=(const FrameBufferManager&) = delete;

    // Reduced buffer size to prevent memory issues
    static constexpr size_t Buffersize = 50; // Reduced from 500
    static constexpr int CLEANUP_INTERVAL = 100; // Default cleanup interval
    
    std::array<FrameData, Buffersize> frames_;
    size_t head_;
    size_t tail_;
    size_t size_;
    mutable std::mutex mutex_;
    std::condition_variable condVar_;
    
    // Cleanup management
    int frameCounter_;
    int cleanupInterval_;
    
    void performCleanup() {
        // Explicitly release all frame memory
        for (size_t i = 0; i < Buffersize; ++i) {
            frames_[i].image.release();
        }
        head_ = 0;
        tail_ = 0;
        size_ = 0;
        
        // Log cleanup for monitoring
        std::cout << "FrameBufferManager: Automatic cleanup performed. Buffer reset." << std::endl;
    }
};

#endif // FRAME_BUFFER_MANAGER_H
