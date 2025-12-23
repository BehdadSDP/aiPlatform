#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

/**
 * @brief Thread-safe queue for producer-consumer pattern
 * 
 * This class provides a lock-free-like interface for inter-thread communication.
 * It replaces shared state with mutex approach for cleaner thread synchronization.
 * 
 * @tparam T The type of elements stored in the queue
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() : shutdown_(false), maxSize_(0) {}
    
    /**
     * @brief Constructor with maximum size limit
     * @param maxSize Maximum number of elements (0 = unlimited)
     */
    explicit ThreadSafeQueue(size_t maxSize) : shutdown_(false), maxSize_(maxSize) {}
    
    ~ThreadSafeQueue() {
        shutdown();
    }

    /**
     * @brief Push an element to the queue (blocking if full)
     * @param item The item to push
     * @return true if pushed successfully, false if shutdown
     */
    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait if queue is full (only if maxSize_ > 0)
        if (maxSize_ > 0) {
            notFull_.wait(lock, [this]() {
                return shutdown_ || queue_.size() < maxSize_;
            });
        }
        
        if (shutdown_) {
            return false;
        }
        
        queue_.push(std::move(item));
        lock.unlock();
        notEmpty_.notify_one();
        return true;
    }

    /**
     * @brief Try to push an element without blocking
     * @param item The item to push
     * @return true if pushed successfully, false if full or shutdown
     */
    bool tryPush(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (shutdown_) {
            return false;
        }
        
        if (maxSize_ > 0 && queue_.size() >= maxSize_) {
            return false;
        }
        
        queue_.push(std::move(item));
        notEmpty_.notify_one();
        return true;
    }

    /**
     * @brief Push and drop oldest if full (for real-time applications)
     * @param item The item to push
     * @return true if pushed successfully, false if shutdown
     */
    bool pushOverwrite(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (shutdown_) {
            return false;
        }
        
        // Drop oldest if at capacity
        if (maxSize_ > 0 && queue_.size() >= maxSize_) {
            queue_.pop();
        }
        
        queue_.push(std::move(item));
        notEmpty_.notify_one();
        return true;
    }

    /**
     * @brief Pop an element from the queue (blocking)
     * @return The element, or std::nullopt if shutdown
     */
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        notEmpty_.wait(lock, [this]() {
            return shutdown_ || !queue_.empty();
        });
        
        if (shutdown_ && queue_.empty()) {
            return std::nullopt;
        }
        
        T item = std::move(queue_.front());
        queue_.pop();
        
        lock.unlock();
        notFull_.notify_one();
        return item;
    }

    /**
     * @brief Try to pop an element without blocking
     * @return The element, or std::nullopt if empty
     */
    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.empty()) {
            return std::nullopt;
        }
        
        T item = std::move(queue_.front());
        queue_.pop();
        notFull_.notify_one();
        return item;
    }

    /**
     * @brief Pop with timeout
     * @param timeout Maximum time to wait
     * @return The element, or std::nullopt if timeout or shutdown
     */
    template<typename Rep, typename Period>
    std::optional<T> popFor(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        bool success = notEmpty_.wait_for(lock, timeout, [this]() {
            return shutdown_ || !queue_.empty();
        });
        
        if (!success || (shutdown_ && queue_.empty())) {
            return std::nullopt;
        }
        
        T item = std::move(queue_.front());
        queue_.pop();
        
        lock.unlock();
        notFull_.notify_one();
        return item;
    }

    /**
     * @brief Get the latest item, discarding older ones (for real-time)
     * @return The latest element, or std::nullopt if empty
     */
    std::optional<T> popLatest() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.empty()) {
            return std::nullopt;
        }
        
        // Keep popping until we get the last one
        T item;
        while (!queue_.empty()) {
            item = std::move(queue_.front());
            queue_.pop();
        }
        
        notFull_.notify_all();
        return item;
    }

    /**
     * @brief Check if the queue is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /**
     * @brief Get the current size of the queue
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * @brief Clear all elements from the queue
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
        notFull_.notify_all();
    }

    /**
     * @brief Signal shutdown to all waiting threads
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    /**
     * @brief Reset the queue for reuse after shutdown
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = false;
        std::queue<T> empty;
        std::swap(queue_, empty);
    }

    /**
     * @brief Check if shutdown has been signaled
     */
    bool isShutdown() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return shutdown_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    std::queue<T> queue_;
    bool shutdown_;
    size_t maxSize_;
};

#endif // THREAD_SAFE_QUEUE_H
