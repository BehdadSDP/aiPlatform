#pragma once
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>

struct SharedDetection {
    cv::Rect box;
    bool valid;
    uint64_t frameSeq;
    cv::Mat frame;
    bool newDetection;
    int classId; // Added to store class ID
};

class SingleObjectData
{
public:
    SingleObjectData() {
        detection.valid = false;
        detection.newDetection = false;
        detection.frameSeq = 0;
        detection.classId = -1; // Initialize to invalid
    }

    SharedDetection detection;
    std::mutex mtx;
    std::condition_variable cv;
};
