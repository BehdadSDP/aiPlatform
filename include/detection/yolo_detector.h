#pragma once

#include "../model.h"
#include <opencv2/dnn.hpp>

namespace ai {

class YoloDetector {
public:
    YoloDetector();
    ~YoloDetector() = default;

    bool initialize(const std::string& model_path, const std::string& config_path);
    std::vector<model::Detection> detect(const cv::Mat& frame);
    std::string getName() const { return "YOLO"; }
    void setConfidenceThreshold(float threshold);
    float getConfidenceThreshold() const;
    std::vector<std::string> getSupportedClasses() const;

private:
    cv::dnn::Net net_;
    float confidence_threshold_;
    float nms_threshold_;
    std::vector<std::string> class_names_;
    std::vector<cv::Scalar> colors_;
    
    void preprocess(const cv::Mat& frame, cv::Mat& blob);
    std::vector<model::Detection> postprocess(const cv::Mat& frame, const std::vector<cv::Mat>& outputs);
    void loadClassNames(const std::string& names_file);
    void generateColors();
};

} // namespace ai
