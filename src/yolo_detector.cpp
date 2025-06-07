#include "include/yolo_detector.h"
#include <fstream>
#include <sstream>

namespace ai {

YoloDetector::YoloDetector()
    : confidence_threshold_(0.5f)
    , nms_threshold_(0.4f) {
}

bool YoloDetector::initialize(const std::string& model_path, const std::string& config_path) {
    try {
        net_ = cv::dnn::readNetFromDarknet(config_path, model_path);
        if (net_.empty()) {
            return false;
        }

        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        // Load class names
        std::string names_file = config_path.substr(0, config_path.find_last_of('.')) + ".names";
        loadClassNames(names_file);
        generateColors();

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::vector<Detection> YoloDetector::detect(const cv::Mat& frame) {
    cv::Mat blob;
    preprocess(frame, blob);

    std::vector<cv::Mat> outputs;
    std::vector<cv::String> out_names = net_.getUnconnectedOutLayersNames();
    net_.setInput(blob);
    net_.forward(outputs, out_names);

    return postprocess(frame, outputs);
}

void YoloDetector::setConfidenceThreshold(float threshold) {
    confidence_threshold_ = threshold;
}

float YoloDetector::getConfidenceThreshold() const {
    return confidence_threshold_;
}

std::vector<std::string> YoloDetector::getSupportedClasses() const {
    return class_names_;
}

void YoloDetector::preprocess(const cv::Mat& frame, cv::Mat& blob) {
    cv::dnn::blobFromImage(frame, blob, 1/255.0, cv::Size(416, 416), cv::Scalar(0,0,0), true, false);
}

std::vector<Detection> YoloDetector::postprocess(const cv::Mat& frame, const std::vector<cv::Mat>& outputs) {
    std::vector<Detection> detections;
    std::vector<int> class_ids;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    float* data = (float*)outputs[0].data;
    const int dimensions = 85;  // 80 classes + 4 box coordinates + 1 confidence
    const int rows = outputs[0].size[1];

    for (int i = 0; i < rows; ++i) {
        float confidence = data[4];
        if (confidence >= confidence_threshold_) {
            float* classes_scores = data + 5;
            cv::Mat scores(1, class_names_.size(), CV_32FC1, classes_scores);
            cv::Point class_id;
            double max_class_score;
            cv::minMaxLoc(scores, 0, &max_class_score, 0, &class_id);

            if (max_class_score > confidence_threshold_) {
                float x = data[0];
                float y = data[1];
                float w = data[2];
                float h = data[3];

                int left = int((x - 0.5 * w) * frame.cols);
                int top = int((y - 0.5 * h) * frame.rows);
                int width = int(w * frame.cols);
                int height = int(h * frame.rows);

                boxes.push_back(cv::Rect(left, top, width, height));
                confidences.push_back((float)max_class_score);
                class_ids.push_back(class_id.x);
            }
        }
        data += dimensions;
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confidence_threshold_, nms_threshold_, indices);

    for (size_t i = 0; i < indices.size(); ++i) {
        int idx = indices[i];
        Detection det;
        det.bbox = boxes[idx];
        det.confidence = confidences[idx];
        det.class_id = class_ids[idx];
        det.class_name = class_names_[class_ids[idx]];
        detections.push_back(det);
    }

    return detections;
}

void YoloDetector::loadClassNames(const std::string& names_file) {
    std::ifstream file(names_file);
    std::string line;
    while (std::getline(file, line)) {
        class_names_.push_back(line);
    }
}

void YoloDetector::generateColors() {
    colors_.resize(class_names_.size());
    for (size_t i = 0; i < class_names_.size(); ++i) {
        colors_[i] = cv::Scalar(rand() % 255, rand() % 255, rand() % 255);
    }
}

} // namespace ai 