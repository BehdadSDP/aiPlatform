#include "model.h"
#include <stdexcept>
#include <iomanip>

model::model(const std::string& configPath, const std::string& weightsPath, const std::string& namesPath) {
    // Load YOLO-Tiny model
    yoloNet_ = cv::dnn::readNetFromDarknet(configPath, weightsPath);
    if (yoloNet_.empty()) {
        throw std::runtime_error("Failed to load YOLO-Tiny model from " + configPath + " and " + weightsPath);
    }
    yoloNet_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    yoloNet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    // Load class names
    std::ifstream classFile(namesPath);
    if (!classFile) {
        throw std::runtime_error("Failed to load class names from " + namesPath);
    }
    std::string line;
    while (std::getline(classFile, line)) {
        classNames_.push_back(line);
    }
}

void model::printMessage(const std::string& message) const {
    std::cout << message << std::endl;
}

void model::detectAndDisplay(std::vector<cv::Mat>& frames) {
    if (frames.empty()) {
        printMessage("No frames to process with YOLO");
        return;
    }

    printMessage("Processing " + std::to_string(frames.size()) + " frames with YOLOv4-Tiny");

    // YOLO parameters
    float confThreshold = 0.5; // Confidence threshold
    float nmsThreshold = 0.4;  // Non-maximum suppression threshold
    int inputWidth = 416;      // YOLO input size
    int inputHeight = 416;

    for (auto& frame : frames) {
        // Preprocess frame for YOLO
        cv::Mat blob;
        cv::dnn::blobFromImage(frame, blob, 1.0 / 255.0, cv::Size(inputWidth, inputHeight), cv::Scalar(), true, false);
        yoloNet_.setInput(blob);

        // Run inference
        std::vector<cv::Mat> outs;
        std::vector<std::string> layerNames = yoloNet_.getUnconnectedOutLayersNames();
        yoloNet_.forward(outs, layerNames);

        // Post-process detections
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;

        for (const auto& out : outs) {
            float* data = (float*)out.data;
            for (int j = 0; j < out.rows; ++j, data += out.cols) {
                cv::Mat scores = out.row(j).colRange(5, out.cols);
                cv::Point classIdPoint;
                double confidence;
                cv::minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
                if (confidence > confThreshold) {
                    int centerX = (int)(data[0] * frame.cols);
                    int centerY = (int)(data[1] * frame.rows);
                    int width = (int)(data[2] * frame.cols);
                    int height = (int)(data[3] * frame.rows);
                    int left = centerX - width / 2;
                    int top = centerY - height / 2;

                    classIds.push_back(classIdPoint.x);
                    confidences.push_back((float)confidence);
                    boxes.push_back(cv::Rect(left, top, width, height));
                }
            }
        }

        // Apply Non-Maximum Suppression
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

        // Draw detections on the frame and output to console
        for (int idx : indices) {
            cv::Rect box = boxes[idx];
            std::string label = classNames_[classIds[idx]] + ": " +
                                std::to_string(confidences[idx]).substr(0, 4);

            // Console output
            std::cout << "Detected: " << label << " at (" << box.x << ", " << box.y << ", "
                      << box.width << ", " << box.height << ")" << std::endl;

            // Draw bounding box and label on the frame
            cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2); // Green box
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
            cv::rectangle(frame,
                          cv::Point(box.x, box.y - textSize.height - 5),
                          cv::Point(box.x + textSize.width, box.y),
                          cv::Scalar(0, 255, 0), cv::FILLED);
            cv::putText(frame, label, cv::Point(box.x, box.y - 5),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1); // Black text
        }

        // Display the frame with detections
        cv::imshow("YOLOv4-Tiny Detections", frame);
        cv::waitKey(1); // Adjust delay as needed
    }
}
