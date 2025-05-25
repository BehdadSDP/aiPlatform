#ifndef MODEL_H
#define MODEL_H

#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/dnn.hpp>
#include <vector>
#include <string>
#include <fstream>

class model
{
public:
    model();
public:
    model(const std::string& onnxPath, const std::string& namesPath, int targetClassId = 0);
    void detectAndDisplay(std::vector<cv::Mat>& frames);

    // Return bounding boxes from YOLO
    // Each "Detection" can store box, confidence, classId, etc.
    struct Detection {
        cv::Rect box;
        float confidence;
        int classId;
    };

    // A new function to detect objects but NOT draw them
    std::vector<Detection> detect(const cv::Mat &frame);

private:
    cv::dnn::Net yoloNet_;
    std::vector<std::string> classNames_;
    void printMessage(const std::string& message) const;
    int targetClassId_; // New member to store the target class ID
};

#endif // MODEL_H
