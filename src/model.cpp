// model.cpp

#include "include/model.h"
#include <stdexcept>
#include <iomanip> // For std::fixed, std::setprecision if needed for printing
#include <fstream> // For std::ifstream
#include <iostream> // For std::cout, std::cerr
#include <string>    // For std::string, std::to_string
#include <vector>    // For std::vector
#include <cmath>     // For std::exp (if needed)
#include <algorithm> // For std::max/min
#include <cfloat>    // For FLT_MAX
#include <chrono>    // For std::chrono

model::model(const std::string &onnxPath, const std::string &namesPath, int targetClassId)
    : targetClassId_(targetClassId) // Initialize targetClassId_
{
    yoloNet_ = cv::dnn::readNetFromONNX(onnxPath);
    if (yoloNet_.empty()) {
        throw std::runtime_error("Failed to load YOLO ONNX model from " + onnxPath);
    }
    
    // Optimize for Raspberry Pi 5 ARM64 - try multiple backends for best performance
    bool backendSet = false;
    
    // Try OpenVINO first (Intel optimizations for ARM)
    try {
        #ifdef CV_DNN_BACKEND_INFERENCE_ENGINE
        yoloNet_.setPreferableBackend(cv::dnn::DNN_BACKEND_INFERENCE_ENGINE);
        yoloNet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        std::cout << "Using OpenVINO backend for inference" << std::endl;
        backendSet = true;
        #else
        std::cout << "OpenVINO backend not available in this OpenCV build" << std::endl;
        #endif
    } catch (const cv::Exception& e) {
        std::cout << "OpenVINO not available, trying alternatives..." << std::endl;
    }
    
    // Try ONNX Runtime backend (usually faster than OpenCV)
    if (!backendSet) {
        try {
            // Check if ONNX backend is available (OpenCV 4.5.4+)
            #ifdef CV_DNN_BACKEND_ONNX
            yoloNet_.setPreferableBackend(cv::dnn::DNN_BACKEND_ONNX);
            yoloNet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            std::cout << "Using ONNX Runtime backend for inference" << std::endl;
            backendSet = true;
            #else
            std::cout << "ONNX Runtime backend not available in this OpenCV build" << std::endl;
            #endif
        } catch (const cv::Exception& e) {
            std::cout << "ONNX Runtime not available, using OpenCV..." << std::endl;
        }
    }
    
    // Fallback to optimized OpenCV backend
    if (!backendSet) {
        yoloNet_.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        yoloNet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        std::cout << "Using optimized OpenCV backend for inference" << std::endl;
    }

    std::ifstream classFile(namesPath);
    if (!classFile.is_open()) {
        throw std::runtime_error("Failed to open class names file: " + namesPath);
    }
    std::string line;
    while (std::getline(classFile, line)) {
        // Basic trim (remove leading/trailing spaces)
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (!line.empty()) { // Avoid adding empty lines
           classNames_.push_back(line);
        }
    }
    classFile.close();

    std::cout << "Loaded " << classNames_.size() << " class names." << std::endl;
    // Optional: Print loaded classes for verification
    // for (size_t i = 0; i < classNames_.size(); ++i) {
    //     std::cout << "Class " << i << ": " << classNames_[i] << std::endl;
    // }

    if (classNames_.empty()) {
        throw std::runtime_error("No class names loaded from " + namesPath);
    }

    // ============================================================ //
    // == Start Replace Block: Corrected Validation for targetClassId == //
    // ============================================================ //

    // Check only if targetClassId is not the special value -1
    if (targetClassId_ != -1) {
        // Check lower bound (signed vs signed is safe)
        bool isInvalid = (targetClassId_ < 0);

        // Check upper bound only if lower bound was okay
        // Compare unsigned vs unsigned for safety
        if (!isInvalid) {
            // Cast the non-negative targetClassId_ to the vector's size type
            // decltype(classNames_.size()) gets the correct unsigned type (e.g., size_t)
            // This comparison avoids the signed/unsigned warning
            isInvalid = (static_cast<decltype(classNames_.size())>(targetClassId_) >= classNames_.size());
        }

        // If either check found it invalid, throw the error
        if (isInvalid) {
            // Construct error message safely (handle empty vector case for size()-1)
            std::string upper_bound_str = (classNames_.empty()) ? "N/A" : std::to_string(classNames_.size() - 1);
            throw std::runtime_error("Invalid target_class_id: " + std::to_string(targetClassId_) +
                                     ". Must be -1 (all classes) or between 0 and " + upper_bound_str);
        }
    }
    // ============================================================ //
    // == End Replace Block: Corrected Validation for targetClassId ==== //
    // ============================================================ //


    // Print status (which is now safe after validation)
    if (targetClassId_ == -1) {
         std::cout << "Target class set to: All Classes" << std::endl;
    } else {
         // This access is now safe due to the validation above
         std::cout << "Target class set to: " << classNames_[targetClassId_]
                   << " (ID: " << targetClassId_ << ")" << std::endl;
    }
} // End of constructor

// Updated detect function for 1x84x8400 output shape
std::vector<model::Detection> model::detect(const cv::Mat &frame)
{
    if (frame.empty()) {
        std::cout << "[WARN] Input frame is empty!" << std::endl;
        return {};
    }

    // Start total timing
    auto start_total = std::chrono::high_resolution_clock::now();

    // --- Constants ---
    const float INPUT_WIDTH = 640.0f;
    const float INPUT_HEIGHT = 640.0f;
    const float SCORE_THRESHOLD = 0.2f;
    const float NMS_THRESHOLD = 0.2f;

    // Step 1: Preprocess frame into blob
    auto start_preprocess = std::chrono::high_resolution_clock::now();
    cv::Mat blob;
    cv::dnn::blobFromImage(frame, blob, 1.0f / 255.0f, cv::Size(INPUT_WIDTH, INPUT_HEIGHT), cv::Scalar(), true, false);
    auto end_preprocess = std::chrono::high_resolution_clock::now();

    // Step 2: Set network input and perform inference
    auto start_inference = std::chrono::high_resolution_clock::now();
    yoloNet_.setInput(blob);
    std::vector<cv::Mat> outs;
    try {
        yoloNet_.forward(outs);
    } catch (const cv::Exception& e) {
        std::cerr << "[ERROR] OpenCV DNN forward pass failed: " << e.what() << std::endl;
        return {};
    }
    auto end_inference = std::chrono::high_resolution_clock::now();

    if (outs.empty() || outs[0].empty()) {
        std::cout << "[DEBUG] No output or empty output from network!" << std::endl;
        return {};
    }

    // Step 3: Parse outputs - optimized post-processing
    auto start_postprocess = std::chrono::high_resolution_clock::now();
    
    cv::Mat output_buffer = outs[0];

    // Validate output format (simplified)
    if (output_buffer.type() != CV_32F || output_buffer.dims < 2) {
        std::cerr << "[ERROR] Invalid output format" << std::endl;
        return {};
    }

    // Handle reshape if needed
    if (output_buffer.dims == 2) {
        int expected_size = (4 + static_cast<int>(classNames_.size())) * 8400;
        if (output_buffer.cols == expected_size) {
            output_buffer = output_buffer.reshape(1, {1, static_cast<int>(4 + classNames_.size()), 8400});
        }
    }

    const int num_proposals = output_buffer.size[2];
    const int num_classes = static_cast<int>(classNames_.size());
    const float* data = reinterpret_cast<const float*>(output_buffer.data);

    // Pre-calculate scaling factors
    const float x_factor = frame.cols / INPUT_WIDTH;
    const float y_factor = frame.rows / INPUT_HEIGHT;

    // Optimized detection processing - reserve memory to avoid reallocations
    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;
    classIds.reserve(100);  // Reasonable initial size
    confidences.reserve(100);
    boxes.reserve(100);

    // Optimized loop - process proposals more efficiently
    for (int i = 0; i < num_proposals; ++i) {
        // Extract bounding box data
        const float cx = data[i];
        const float cy = data[num_proposals + i];
        const float w = data[2 * num_proposals + i];
        const float h = data[3 * num_proposals + i];

        // Find max class score more efficiently
        float max_score = -1.0f;
        int best_class = -1;
        
        const float* class_scores = &data[4 * num_proposals + i];  // Start of class scores for this proposal
        for (int j = 0; j < num_classes; ++j) {
            const float score = class_scores[j * num_proposals];
            if (score > max_score) {
                max_score = score;
                best_class = j;
            }
        }

        // Early exit if confidence too low
        if (max_score < SCORE_THRESHOLD) {
            continue;
        }

        // Convert coordinates to frame space
        const float left = (cx - w * 0.5f) * x_factor;
        const float top = (cy - h * 0.5f) * y_factor;
        const float width = w * x_factor;
        const float height = h * y_factor;

        // Clamp to frame bounds
        const int x = static_cast<int>(std::max(0.0f, std::min(left, static_cast<float>(frame.cols - 1))));
        const int y = static_cast<int>(std::max(0.0f, std::min(top, static_cast<float>(frame.rows - 1))));
        const int w_clamped = static_cast<int>(std::max(1.0f, std::min(width, static_cast<float>(frame.cols - x))));
        const int h_clamped = static_cast<int>(std::max(1.0f, std::min(height, static_cast<float>(frame.rows - y))));

        if (w_clamped > 0 && h_clamped > 0) {
            boxes.emplace_back(x, y, w_clamped, h_clamped);
            confidences.push_back(max_score);
            classIds.push_back(best_class);
        }
    }

    // Step 4: Apply NMS
    std::vector<int> nms_indices;
    if (!boxes.empty()) {
        cv::dnn::NMSBoxes(boxes, confidences, SCORE_THRESHOLD, NMS_THRESHOLD, nms_indices);
    }

    // Step 5: Create final detections
    std::vector<Detection> final_detections;
    final_detections.reserve(nms_indices.size());

    for (int idx : nms_indices) {
        Detection det;
        det.box = boxes[idx];
        det.confidence = confidences[idx];
        det.classId = classIds[idx];
        final_detections.push_back(det);
    }
    return final_detections;
}
