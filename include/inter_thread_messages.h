#ifndef INTER_THREAD_MESSAGES_H
#define INTER_THREAD_MESSAGES_H

#include <opencv2/opencv.hpp>
#include <chrono>
#include <variant>
#include <cstdint>

/**
 * @brief Messages for inter-thread communication in the AI Platform
 * 
 * These message types replace shared state with mutex pattern,
 * providing cleaner and safer thread synchronization using
 * the producer-consumer pattern.
 */

namespace messages {

/**
 * @brief Message from detection thread to tracking thread
 * Contains detection result for tracker initialization
 */
struct DetectionResult {
    cv::Rect boundingBox;           // Detected object bounding box
    cv::Mat frame;                  // Frame for tracker initialization (cloned)
    uint64_t frameSequence;         // Frame sequence number for ordering
    int classId;                    // Detected object class ID
    float confidence;               // Detection confidence score
    std::chrono::steady_clock::time_point timestamp;  // Detection timestamp
    
    DetectionResult() 
        : frameSequence(0), classId(-1), confidence(0.0f),
          timestamp(std::chrono::steady_clock::now()) {}
    
    DetectionResult(const cv::Rect& box, const cv::Mat& frm, uint64_t seq, 
                   int cls, float conf = 1.0f)
        : boundingBox(box), frame(frm.clone()), frameSequence(seq),
          classId(cls), confidence(conf),
          timestamp(std::chrono::steady_clock::now()) {}
    
    // Move constructor for efficiency
    DetectionResult(DetectionResult&& other) noexcept
        : boundingBox(other.boundingBox),
          frame(std::move(other.frame)),
          frameSequence(other.frameSequence),
          classId(other.classId),
          confidence(other.confidence),
          timestamp(other.timestamp) {}
    
    // Move assignment
    DetectionResult& operator=(DetectionResult&& other) noexcept {
        if (this != &other) {
            boundingBox = other.boundingBox;
            frame = std::move(other.frame);
            frameSequence = other.frameSequence;
            classId = other.classId;
            confidence = other.confidence;
            timestamp = other.timestamp;
        }
        return *this;
    }
    
    // Copy constructor (explicit clone)
    DetectionResult(const DetectionResult& other)
        : boundingBox(other.boundingBox),
          frame(other.frame.clone()),
          frameSequence(other.frameSequence),
          classId(other.classId),
          confidence(other.confidence),
          timestamp(other.timestamp) {}
    
    // Copy assignment
    DetectionResult& operator=(const DetectionResult& other) {
        if (this != &other) {
            boundingBox = other.boundingBox;
            frame = other.frame.clone();
            frameSequence = other.frameSequence;
            classId = other.classId;
            confidence = other.confidence;
            timestamp = other.timestamp;
        }
        return *this;
    }
};

/**
 * @brief Message from tracking thread to detection thread
 * Signals tracker state changes
 */
struct TrackerStatus {
    enum class State {
        IDLE,           // Tracker not initialized
        TRACKING,       // Actively tracking
        LOST,           // Tracking lost, needs re-detection
        FAILED          // Tracker failed completely
    };
    
    State state;
    cv::Rect lastKnownBox;          // Last known bounding box
    cv::Mat lastFrame;              // Last frame when tracking was lost
    float confidence;               // Tracking confidence when lost
    std::chrono::steady_clock::time_point timestamp;
    
    TrackerStatus() 
        : state(State::IDLE), confidence(0.0f),
          timestamp(std::chrono::steady_clock::now()) {}
    
    TrackerStatus(State s, const cv::Rect& box = cv::Rect(), 
                  const cv::Mat& frame = cv::Mat(), float conf = 0.0f)
        : state(s), lastKnownBox(box), 
          lastFrame(frame.empty() ? cv::Mat() : frame.clone()),
          confidence(conf),
          timestamp(std::chrono::steady_clock::now()) {}
    
    // Move semantics
    TrackerStatus(TrackerStatus&& other) noexcept
        : state(other.state),
          lastKnownBox(other.lastKnownBox),
          lastFrame(std::move(other.lastFrame)),
          confidence(other.confidence),
          timestamp(other.timestamp) {}
    
    TrackerStatus& operator=(TrackerStatus&& other) noexcept {
        if (this != &other) {
            state = other.state;
            lastKnownBox = other.lastKnownBox;
            lastFrame = std::move(other.lastFrame);
            confidence = other.confidence;
            timestamp = other.timestamp;
        }
        return *this;
    }
    
    bool isTracking() const { return state == State::TRACKING; }
    bool needsRedetection() const { return state == State::LOST || state == State::FAILED; }
};

/**
 * @brief Message for tracking results to navigation thread
 */
struct TrackingResult {
    cv::Rect trackedBox;            // Current tracked bounding box
    cv::Point2f center;             // Center of tracked object
    int frameWidth;                 // Frame width for error calculation
    int frameHeight;                // Frame height for error calculation
    float confidence;               // Tracking confidence
    bool valid;                     // Whether tracking result is valid
    std::chrono::steady_clock::time_point timestamp;
    
    TrackingResult()
        : frameWidth(0), frameHeight(0), confidence(0.0f), valid(false),
          timestamp(std::chrono::steady_clock::now()) {}
    
    TrackingResult(const cv::Rect& box, int width, int height, float conf = 1.0f)
        : trackedBox(box),
          center(box.x + box.width / 2.0f, box.y + box.height / 2.0f),
          frameWidth(width), frameHeight(height),
          confidence(conf), valid(true),
          timestamp(std::chrono::steady_clock::now()) {}
    
    // Move semantics (lightweight struct, default is fine)
    TrackingResult(TrackingResult&&) = default;
    TrackingResult& operator=(TrackingResult&&) = default;
    TrackingResult(const TrackingResult&) = default;
    TrackingResult& operator=(const TrackingResult&) = default;
};

/**
 * @brief Command message for control flow between threads
 */
struct ControlCommand {
    enum class Type {
        START_DETECTION,    // Start detection process
        STOP_DETECTION,     // Stop detection process
        START_TRACKING,     // Start tracking with provided detection
        STOP_TRACKING,      // Stop tracking
        RESET,              // Reset all states
        SHUTDOWN            // Shutdown threads
    };
    
    Type type;
    std::chrono::steady_clock::time_point timestamp;
    
    ControlCommand(Type t = Type::RESET)
        : type(t), timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Variant type for all possible inter-thread messages
 */
using InterThreadMessage = std::variant<
    DetectionResult,
    TrackerStatus,
    TrackingResult,
    ControlCommand
>;

} // namespace messages

#endif // INTER_THREAD_MESSAGES_H
