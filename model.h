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
    // Constructor for Darknet models (YOLOv4)
    model(const std::string& configPath, const std::string& weightsPath, const std::string& namesPath);
    
    // Constructor for ONNX models (YOLOv12m)
    model(const std::string& onnxPath, const std::string& namesPath);
    
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
    bool isOnnxModel_ = false;
};

#endif // MODEL_H
