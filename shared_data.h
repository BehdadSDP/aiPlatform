#pragma once
#include <mutex>
#include <opencv2/opencv.hpp>

struct SharedDetection {
    cv::Rect box;
    bool valid;
    uint64_t frameSeq; // or int64_t timestamp
};

class SingleObjectData
{
public:
    // The bounding box from YOLO
    SharedDetection detection;

    // Protect detection with a mutex
    std::mutex mtx;
};

