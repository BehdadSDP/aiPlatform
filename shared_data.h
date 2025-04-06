#pragma once
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>

struct SharedDetection {
    cv::Rect box;
    bool valid;
    uint64_t frameSeq; // or int64_t timestamp
    cv::Mat frame;     // Store the frame where the detection occurred
    bool newDetection; // Flag for new detection (if using previous modification)
};

class SingleObjectData
{
public:
    SingleObjectData() {
        detection.valid = false;
        detection.newDetection = false; // If using previous modification
        detection.frameSeq = 0;
    }

    // The bounding box and frame from YOLO
    SharedDetection detection;

    // Protect detection with a mutex and condition variable
    std::mutex mtx;
    std::condition_variable cv; // For notifying tracker (optional, from previous mod)
};
