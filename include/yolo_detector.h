#pragma once

#include "detector.h"
#include <opencv2/dnn.hpp>

namespace ai {

class YoloDetector : public Detector {
public:
    YoloDetector();
    ~YoloDetector() override = default;

    bool initialize(const std::string& model_path, const std::string& config_path) override;
    std::vector<Detection> detect(const cv::Mat& frame) override;
    std::string getName() const override { return "YOLO"; }
    void setConfidenceThreshold(float threshold) override;
    float getConfidenceThreshold() const override;
    std::vector<std::string> getSupportedClasses() const override;

private:
    cv::dnn::Net net_;
    float confidence_threshold_;
    float nms_threshold_;
    std::vector<std::string> class_names_;
    std::vector<cv::Scalar> colors_;
    
    void preprocess(const cv::Mat& frame, cv::Mat& blob);
    std::vector<Detection> postprocess(const cv::Mat& frame, const std::vector<cv::Mat>& outputs);
    void loadClassNames(const std::string& names_file);
    void generateColors();
};

} // namespace ai 