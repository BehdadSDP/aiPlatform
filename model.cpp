#include "model.h"
#include <stdexcept>
#include <iomanip>

// Constructor (unchanged)
model::model(const std::string &configPath,
             const std::string &weightsPath,
             const std::string &namesPath)
{
    yoloNet_ = cv::dnn::readNetFromDarknet(configPath, weightsPath);
    if (yoloNet_.empty()) {
        throw std::runtime_error("Failed to load YOLO model from " + configPath + " and " + weightsPath);
    }
    yoloNet_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    yoloNet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    std::ifstream classFile(namesPath);
    if (!classFile.is_open()) {
        throw std::runtime_error("Failed to open class names file: " + namesPath);
    }
    std::string line;
    while (std::getline(classFile, line)) {
        classNames_.push_back(line);
    }
    classFile.close();
}

std::vector<model::Detection> model::detect(const cv::Mat &frame)
{
    if (frame.empty()) {
        return {};
    }

    // Preprocess
    float scale = 1.0f / 255.0f;
    cv::Size yoloSize(416, 416);
    cv::Mat blob;
    cv::dnn::blobFromImage(frame, blob, scale, yoloSize, cv::Scalar(), true, false);
    yoloNet_.setInput(blob);

    // Forward
    std::vector<cv::Mat> outs;
    std::vector<std::string> layerNames = yoloNet_.getUnconnectedOutLayersNames();
    yoloNet_.forward(outs, layerNames);

    // Post-process
    float confThreshold = 0.5;
    float nmsThreshold = 0.4;
    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    for (auto &out : outs) {
        float* data = (float*) out.data;
        for (int j = 0; j < out.rows; ++j, data += out.cols) {
            cv::Mat scores = out.row(j).colRange(5, out.cols);
            cv::Point classIdPoint;
            double confidence;
            cv::minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
            if (confidence > confThreshold && classIdPoint.x == 0) { // Filter for "person" (class ID 0)
                int centerX = (int)(data[0] * frame.cols);
                int centerY = (int)(data[1] * frame.rows);
                int width   = (int)(data[2] * frame.cols);
                int height  = (int)(data[3] * frame.rows);
                int left    = centerX - width / 2;
                int top     = centerY - height / 2;

                classIds.push_back(classIdPoint.x);
                confidences.push_back((float)confidence);
                boxes.push_back(cv::Rect(left, top, width, height));
            }
        }
    }

    // Apply Non-Maximum Suppression
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

    // Gather final detections
    std::vector<Detection> detections;
    detections.reserve(indices.size());
    for (int idx : indices) {
        Detection det;
        det.box        = boxes[idx];
        det.confidence = confidences[idx];
        det.classId    = classIds[idx];
        detections.push_back(det);
    }

    return detections;
}
