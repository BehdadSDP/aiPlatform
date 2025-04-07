#include "model.h"
#include <stdexcept>
#include <iomanip>

// Default constructor
model::model() : isOnnxModel_(false) {
    // Initialize with empty model
}

// Constructor for Darknet models (YOLOv4)
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
    isOnnxModel_ = false;

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

// Constructor for ONNX models (YOLOv12m)
model::model(const std::string &onnxPath, const std::string &namesPath)
{
    try {
        yoloNet_ = cv::dnn::readNetFromONNX(onnxPath);
        if (yoloNet_.empty()) {
            throw std::runtime_error("Failed to load ONNX model from " + onnxPath);
        }
        yoloNet_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        yoloNet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        isOnnxModel_ = true;

        std::ifstream classFile(namesPath);
        if (!classFile.is_open()) {
            throw std::runtime_error("Failed to open class names file: " + namesPath);
        }
        std::string line;
        while (std::getline(classFile, line)) {
            classNames_.push_back(line);
        }
        classFile.close();
    } catch (const cv::Exception& e) {
        throw std::runtime_error("OpenCV error loading ONNX model: " + std::string(e.what()));
    }
}

std::vector<model::Detection> model::detect(const cv::Mat &frame)
{
    if (frame.empty()) {
        return {};
    }

    // Preprocess
    float scale = 1.0f / 255.0f;
    cv::Size yoloSize(640, 640); // YOLOv12m typically uses 640x640 input
    cv::Mat blob;
    cv::dnn::blobFromImage(frame, blob, scale, yoloSize, cv::Scalar(), true, false);
    
    try {
        yoloNet_.setInput(blob);

        // Forward
        std::vector<cv::Mat> outs;
        std::vector<std::string> layerNames = yoloNet_.getUnconnectedOutLayersNames();
        yoloNet_.forward(outs, layerNames);

        // Post-process
        float confThreshold = 0.3; // Lowered from 0.5 to detect more objects
        float nmsThreshold = 0.4;
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;

        if (isOnnxModel_) {
            // ONNX model output processing (YOLOv12m)
            std::cout << "Processing ONNX model output..." << std::endl;
            
            for (auto &out : outs) {
                float* data = (float*) out.data;
                
                // Print output shape for debugging
                std::cout << "ONNX output shape: " << out.size << std::endl;
                
                // Check if the output format is [batch, num_classes+5, num_anchors] (common for newer YOLO models)
                if (out.size.dims() == 3 && out.size[0] == 1 && out.size[1] > 5 && out.size[2] > 0) {
                    std::cout << "Detected [batch, num_classes+5, num_anchors] format" << std::endl;
                    int numClasses = out.size[1] - 5;
                    int numAnchors = out.size[2];
                    
                    std::cout << "Number of classes: " << numClasses << ", Number of anchors: " << numAnchors << std::endl;
                    
                    // Process each anchor
                    for (int i = 0; i < numAnchors; i++) {
                        // Get confidence score
                        float confidence = data[4 * numAnchors + i];
                        
                        if (confidence > confThreshold) {
                            // Find the class with highest score
                            float maxClassScore = 0.0f;
                            int maxClassId = 0;
                            
                            for (int j = 0; j < numClasses; j++) {
                                float score = data[(j + 5) * numAnchors + i];
                                if (score > maxClassScore) {
                                    maxClassScore = score;
                                    maxClassId = j;
                                }
                            }
                            
                            // Calculate final confidence
                            float finalConfidence = confidence * maxClassScore;
                            
                            std::cout << "Anchor " << i << ": class=" << maxClassId 
                                      << ", confidence=" << finalConfidence << std::endl;
                            
                            // Check if this is a person class
                            bool isPerson = false;
                            if (maxClassId == 0) {
                                isPerson = true;
                            } else if (!classNames_.empty() && maxClassId < classNames_.size()) {
                                isPerson = (classNames_[maxClassId] == "person");
                            }
                            
                            if (finalConfidence > confThreshold && isPerson) {
                                // Get bounding box coordinates
                                float x = data[0 * numAnchors + i];
                                float y = data[1 * numAnchors + i];
                                float w = data[2 * numAnchors + i];
                                float h = data[3 * numAnchors + i];
                                
                                // Convert normalized coordinates to pixel coordinates
                                int left = static_cast<int>((x - w/2) * frame.cols);
                                int top = static_cast<int>((y - h/2) * frame.rows);
                                int width = static_cast<int>(w * frame.cols);
                                int height = static_cast<int>(h * frame.rows);
                                
                                classIds.push_back(maxClassId);
                                confidences.push_back(finalConfidence);
                                boxes.push_back(cv::Rect(left, top, width, height));
                                
                                std::cout << "Added person detection: class=" << maxClassId 
                                          << ", confidence=" << finalConfidence 
                                          << ", box=" << cv::Rect(left, top, width, height) << std::endl;
                            }
                        }
                    }
                } 
                // Check if the output format is [batch, num_detections, 7] (common for newer YOLO models)
                else if (out.size.dims() == 3 && out.size[0] == 1 && out.size[1] > 0 && out.size[2] == 7) {
                    std::cout << "Detected [batch, num_detections, 7] format" << std::endl;
                    int numDetections = out.size[1];
                    
                    // Format: [image_id, x1, y1, x2, y2, confidence, class_id]
                    for (int i = 0; i < numDetections; i++) {
                        float confidence = data[i * 7 + 5];
                        int classId = static_cast<int>(data[i * 7 + 6]);
                        
                        std::cout << "Detection " << i << ": class=" << classId 
                                  << ", confidence=" << confidence << std::endl;
                        
                        // Check if this is a person class (could be 0 or another index)
                        bool isPerson = false;
                        if (classId == 0) {
                            isPerson = true;
                        } else if (!classNames_.empty() && classId < classNames_.size()) {
                            isPerson = (classNames_[classId] == "person");
                        }
                        
                        if (confidence > confThreshold && isPerson) {
                            // Get bounding box coordinates
                            float x1 = data[i * 7 + 1];
                            float y1 = data[i * 7 + 2];
                            float x2 = data[i * 7 + 3];
                            float y2 = data[i * 7 + 4];
                            
                            // Convert normalized coordinates to pixel coordinates
                            int left = static_cast<int>(x1 * frame.cols);
                            int top = static_cast<int>(y1 * frame.rows);
                            int width = static_cast<int>((x2 - x1) * frame.cols);
                            int height = static_cast<int>((y2 - y1) * frame.rows);
                            
                            classIds.push_back(classId);
                            confidences.push_back(confidence);
                            boxes.push_back(cv::Rect(left, top, width, height));
                            
                            std::cout << "Added person detection: class=" << classId 
                                      << ", confidence=" << confidence 
                                      << ", box=" << cv::Rect(left, top, width, height) << std::endl;
                        }
                    }
                } 
                // Check if the output format is [batch, num_detections, 85] (common for older YOLO models)
                else if (out.size.dims() == 3 && out.size[0] == 1 && out.size[1] > 0 && out.size[2] > 5) {
                    std::cout << "Detected [batch, num_detections, 85] format" << std::endl;
                    int numDetections = out.size[1];
                    int numClasses = out.size[2] - 5;
                    
                    // Process each detection
                    for (int i = 0; i < numDetections; i++) {
                        float confidence = data[i * out.size[2] + 4];
                        
                        // Find the class with highest score
                        float maxClassScore = 0.0f;
                        int maxClassId = 0;
                        for (int j = 5; j < out.size[2]; j++) {
                            float score = data[i * out.size[2] + j];
                            if (score > maxClassScore) {
                                maxClassScore = score;
                                maxClassId = j - 5;
                            }
                        }
                        
                        std::cout << "Detection " << i << ": class=" << maxClassId 
                                  << ", confidence=" << confidence 
                                  << ", maxClassScore=" << maxClassScore << std::endl;
                        
                        // Check if this is a person class (could be 0 or another index)
                        bool isPerson = false;
                        if (maxClassId == 0) {
                            isPerson = true;
                        } else if (!classNames_.empty() && maxClassId < classNames_.size()) {
                            isPerson = (classNames_[maxClassId] == "person");
                        }
                        
                        if (confidence > confThreshold && isPerson) {
                            float x = data[i * out.size[2]];
                            float y = data[i * out.size[2] + 1];
                            float w = data[i * out.size[2] + 2];
                            float h = data[i * out.size[2] + 3];
                            
                            // Convert normalized coordinates to pixel coordinates
                            int left = static_cast<int>((x - w/2) * frame.cols);
                            int top = static_cast<int>((y - h/2) * frame.rows);
                            int width = static_cast<int>(w * frame.cols);
                            int height = static_cast<int>(h * frame.rows);
                            
                            classIds.push_back(maxClassId);
                            confidences.push_back(confidence);
                            boxes.push_back(cv::Rect(left, top, width, height));
                            
                            std::cout << "Added person detection: class=" << maxClassId 
                                      << ", confidence=" << confidence 
                                      << ", box=" << cv::Rect(left, top, width, height) << std::endl;
                        }
                    }
                } else {
                    std::cout << "Unknown output format" << std::endl;
                    std::cout << "Output dimensions: " << out.size.dims() << std::endl;
                    for (int i = 0; i < out.size.dims(); i++) {
                        std::cout << "Dimension " << i << ": " << out.size[i] << std::endl;
                    }
                }
            }
        } else {
            // Darknet model output processing (YOLOv4)
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
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error during detection: " << e.what() << std::endl;
        return {};
    }
}

void model::detectAndDisplay(std::vector<cv::Mat>& frames) {
    for (auto& frame : frames) {
        std::vector<Detection> detections = detect(frame);
        
        // Draw detections on the frame
        for (const auto& det : detections) {
            cv::rectangle(frame, det.box, cv::Scalar(0, 255, 0), 2);
            std::string label = classNames_.empty() ? "Person" : classNames_[det.classId];
            label += " " + std::to_string(static_cast<int>(det.confidence * 100)) + "%";
            cv::putText(frame, label, cv::Point(det.box.x, det.box.y - 10),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
        }
    }
}

void model::printMessage(const std::string& message) const {
    std::cout << message << std::endl;
}
