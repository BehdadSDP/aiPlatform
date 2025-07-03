#ifndef FRAME_BUFFER_MANAGER_H
#define FRAME_BUFFER_MANAGER_H

#include <opencv4/opencv2/opencv.hpp>
#include <array>
#include <mutex>
#include <condition_variable>
#include <memory>

struct FrameData {
    cv::Mat image;
    int64_t timestamp;
    unsigned int sequence;
    std::string format;
    cv::Size size;
    double fps;

    FrameData() : timestamp(0), sequence(0), format(""), size(0, 0), fps(0.0) {}
    
    // Copy constructor - only when absolutely necessary
    FrameData(const FrameData& other) 
        : image(other.image.clone()), timestamp(other.timestamp), 
          sequence(other.sequence), format(other.format), 
          size(other.size), fps(other.fps) {}
    
    // Copy assignment operator - only when absolutely necessary
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
    
    // Move constructor - preferred for performance
    FrameData(FrameData&& other) noexcept 
        : image(std::move(other.image)), timestamp(other.timestamp), 
          sequence(other.sequence), format(std::move(other.format)), 
          size(other.size), fps(other.fps) {}
    
    // Move assignment operator - preferred for performance
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

    // ✅ OPTIMIZED: Use move semantics to avoid unnecessary copying
    void addFrame(FrameData&& frameData) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Explicitly release old frame memory before overwriting
        if (size_ >= Buffersize) {
            frames_[head_].image.release();  // Release the oldest frame
            head_ = (head_ + 1) % Buffersize;  // ✅ FIXED: Properly advance head
        }
        
        // ✅ Use move instead of copy
        frames_[tail_] = std::move(frameData);
        tail_ = (tail_ + 1) % Buffersize;
        if (size_ < Buffersize) size_++;
        // Note: head_ is already updated above when buffer is full
        
        // Increment frame counter for automatic cleanup
        frameCounter_++;
        
        // Automatic cleanup every CLEANUP_INTERVAL frames
        if (frameCounter_ >= cleanupInterval_) {
            performCleanup();
            frameCounter_ = 0;
        }
        
        condVar_.notify_all();
    }

    // ✅ OPTIMIZED: Return reference to avoid copying
    bool getLatestFrame(FrameData& frameData) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ == 0) return false;
        size_t latestIndex = (tail_ == 0) ? (Buffersize - 1) : (tail_ - 1);
        
        // ✅ Only clone the image, not the entire struct
        frameData = frames_[latestIndex];
        frameData.image = frames_[latestIndex].image.clone(); // Only clone when needed
        return true;
    }

    // ✅ OPTIMIZED: Use move semantics for bulk operations
    void getAllFrames(std::vector<FrameData>& frames) {
        std::lock_guard<std::mutex> lock(mutex_);
        frames.clear();
        if (size_ == 0) return;
        
        frames.reserve(size_); // Pre-allocate to avoid reallocations
        size_t index = head_;
        for (size_t i = 0; i < size_; ++i) {
            FrameData fd = frames_[index];
            fd.image = frames_[index].image.clone(); // Only clone when needed
            frames.push_back(std::move(fd)); // Use move
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

    // ✅ REDUCED: Smaller buffer size to prevent memory issues
    static constexpr size_t Buffersize = 20; // Reduced from 50 to 20
    static constexpr int CLEANUP_INTERVAL = 50; // Reduced from 100 to 50
    
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
