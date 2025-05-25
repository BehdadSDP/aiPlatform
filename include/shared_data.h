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
    int classId;
};

class SingleObjectData
{
public:
    SingleObjectData() {
        detection.valid = false;
        detection.newDetection = false;
        detection.frameSeq = 0;
        detection.classId = -1;
        trackerFailed = false;
    }

    SharedDetection detection;
    bool trackerFailed;
    std::mutex mtx;
    std::condition_variable cv;
};
