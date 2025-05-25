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

model::model(const std::string &onnxPath, const std::string &namesPath, int targetClassId)
    : targetClassId_(targetClassId) // Initialize targetClassId_
{
    yoloNet_ = cv::dnn::readNetFromONNX(onnxPath);
    if (yoloNet_.empty()) {
        throw std::runtime_error("Failed to load YOLO ONNX model from " + onnxPath);
    }
    // Consider using DNN_BACKEND_DEFAULT and DNN_TARGET_CPU for broader compatibility initially
    // Or configure based on available hardware (e.g., OpenVINO for Intel CPU, CUDA for Nvidia)
    yoloNet_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV); // Default backend
    yoloNet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);     // Default target

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

    // --- Constants ---
    // Use consistent naming with main.cpp if possible, or keep internal constants
    const float INPUT_WIDTH = 640.0f;
    const float INPUT_HEIGHT = 640.0f;
    // This is the internal threshold used BEFORE NMS
    // You mentioned lowering this helped with distant objects
    const float SCORE_THRESHOLD = 0.2f;
    const float NMS_THRESHOLD = 0.45f;

    // Step 1: Preprocess frame into blob
    cv::Mat blob;
    // swapRB=true: Assumes model expects RGB, OpenCV reads BGR. Adjust if model expects BGR.
    // crop=false: No cropping during blob creation.
    cv::dnn::blobFromImage(frame, blob, 1.0f / 255.0f, cv::Size(INPUT_WIDTH, INPUT_HEIGHT), cv::Scalar(), true, false);

    // Step 2: Set network input
    yoloNet_.setInput(blob);

    // Step 3: Perform forward pass
    std::vector<cv::Mat> outs;
    try {
        yoloNet_.forward(outs); // Potentially multiple outputs, but YOLO usually has one main detection output
    } catch (const cv::Exception& e) {
        std::cerr << "[ERROR] OpenCV DNN forward pass failed: " << e.what() << std::endl;
        return {};
    }

    if (outs.empty() || outs[0].empty()) {
         std::cout << "[DEBUG] No output or empty output from network!" << std::endl;
         return {};
    }

    // --- Output Parsing for Shape [1, 84, 8400] ---
    // Assuming the primary detection output is the first element in 'outs'
    cv::Mat output_buffer = outs[0];

    // Ensure the output is float32 and has 3 dimensions
     if (output_buffer.type() != CV_32F) {
        std::cerr << "[ERROR] Unexpected output data type: " << output_buffer.type() << " (expected CV_32F)" << std::endl;
        return {};
    }
     if (output_buffer.dims != 3) {
        // Handle cases where output might be flattened (e.g., dims=2) - requires reshape
        if (output_buffer.dims == 2 && output_buffer.rows == 1 && output_buffer.cols == 1 * (4 + classNames_.size()) * 8400) {
             std::cout << "[DEBUG] Reshaping 2D output to 3D." << std::endl;
             // Attempt to reshape assuming [1, 84, 8400] layout flattened
             output_buffer = output_buffer.reshape(1, {1, static_cast<int>(4 + classNames_.size()), 8400});
        } else {
             std::cerr << "[ERROR] Unexpected output dimensions: " << output_buffer.dims << " (expected 3)" << std::endl;
             return {};
        }
    }


    // Get dimensions after potential reshape
    const int batch_size = output_buffer.size[0];    // Should be 1
    const int num_rows = output_buffer.size[1];      // Should be 84 (4 bbox + 80 classes)
    const int num_proposals = output_buffer.size[2]; // Should be 8400

    // Validate dimensions against expectations (using loaded classNames size)
    const int expected_rows = 4 + static_cast<int>(classNames_.size());
    if (batch_size != 1 || num_rows != expected_rows) {
         std::cerr << "[ERROR] Unexpected output dimensions. Expected [1, "
                   << expected_rows << ", num_proposals], got ["
                   << batch_size << ", " << num_rows << ", " << num_proposals << "]" << std::endl;
         // Check if classNames_.size() matches the model (e.g., 80 for COCO)
         if (classNames_.empty()) {
             std::cerr << "  Class names list is empty!" << std::endl;
         } else if (num_rows - 4 != static_cast<int>(classNames_.size())) {
             std::cerr << "  Model output suggests " << (num_rows - 4) << " classes, but loaded " << classNames_.size() << " class names." << std::endl;
         }
         return {};
    }

    const int num_classes = num_rows - 4; // Should match classNames_.size()

    // Step 4: Process Detections
    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    const float* data = reinterpret_cast<const float*>(output_buffer.data);

    // Calculate scaling factors once
    float x_factor = frame.cols / INPUT_WIDTH;
    float y_factor = frame.rows / INPUT_HEIGHT;

    // Iterate through each of the 8400 proposals
    for (int i = 0; i < num_proposals; ++i) {
        // --- Extract data for proposal 'i' using transposed layout ---
        float cx = data[i];                     // Row 0, proposal i
        float cy = data[num_proposals + i];     // Row 1, proposal i
        float w  = data[2 * num_proposals + i]; // Row 2, proposal i
        float h  = data[3 * num_proposals + i]; // Row 3, proposal i

        // Find the class with the highest score for this proposal
        int class_id = -1;
        float max_class_score = -FLT_MAX;

        // Class scores start from row 4
        for (int j = 0; j < num_classes; ++j) {
            // Score for class 'j' is at row (4+j), proposal i
            float score = data[(4 + j) * num_proposals + i];
            if (score > max_class_score) {
                max_class_score = score;
                class_id = j;
            }
        }

        // --- Confidence Calculation ---
        // Based on previous logs, assume the model output (max_class_score)
        // is already a probability [0, 1] because applying sigmoid resulted in ~0.5.
        float confidence = max_class_score;
        // If you were certain the output was logits, you would use:
        // float confidence = 1.0f / (1.0f + std::exp(-max_class_score));

        // Filter based on confidence threshold AND target class
        // Note: Using the internal SCORE_THRESHOLD here.
        // Note: Using the member variable targetClassId_ for filtering.
        if (confidence >= SCORE_THRESHOLD && class_id != -1 && (targetClassId_ == -1 || class_id == targetClassId_))
        {
             // Convert [cx, cy, w, h] (relative to 640x640 input)
             // to [left, top, width, height] (relative to original frame)
            float left = (cx - w / 2.0f) * x_factor;
            float top = (cy - h / 2.0f) * y_factor;
            float width = w * x_factor;
            float height = h * y_factor;

            // Clamp coordinates to frame boundaries to prevent errors
            int finalLeft = static_cast<int>(std::max(0.0f, std::min(left, (float)frame.cols - 1.0f)));
            int finalTop = static_cast<int>(std::max(0.0f, std::min(top, (float)frame.rows - 1.0f)));
            // Ensure width/height are at least 1 after potential clamping/scaling issues
            int finalWidth = static_cast<int>(std::max(1.0f, std::min(width, (float)frame.cols - finalLeft)));
            int finalHeight = static_cast<int>(std::max(1.0f, std::min(height, (float)frame.rows - finalTop)));

             // Add valid boxes to lists for NMS
             if (finalWidth > 0 && finalHeight > 0) {
                boxes.push_back(cv::Rect(finalLeft, finalTop, finalWidth, finalHeight));
                confidences.push_back(confidence);
                classIds.push_back(class_id); // Store the correct class_id found
             }
        }
    } // End loop over proposals

    // Step 5: Apply Non-Maximum Suppression
    std::vector<int> nms_indices;
    if (!boxes.empty()) { // Check if there are boxes to process
        // NMSBoxes filters boxes based on score and overlap
        cv::dnn::NMSBoxes(boxes, confidences, SCORE_THRESHOLD, NMS_THRESHOLD, nms_indices);
    } else {
        //std::cout << "[DEBUG] No boxes passed the score threshold before NMS." << std::endl;
    }


    // Step 6: Create final detection list from NMS results
    std::vector<Detection> final_detections;
    final_detections.reserve(nms_indices.size()); // Optimize memory allocation

    for (int idx : nms_indices) {
        Detection det;
        det.box = boxes[idx];
        det.confidence = confidences[idx]; // Use the confidence score that passed NMS
        det.classId = classIds[idx];       // Use the class ID associated with the box

        // Optional: Debug print for final detections AFTER NMS
        // std::cout << "[DEBUG] Final Detection [" << idx << "]: Class=" << classNames_[det.classId]
        //           << ", Conf=" << det.confidence
        //           << ", Box=" << det.box << std::endl;

        final_detections.push_back(det);
    }

    // Optional: Print number of detections before/after NMS
    // std::cout << "[DEBUG] Detections before NMS: " << boxes.size()
    //           << ", After NMS: " << final_detections.size() << std::endl;


    return final_detections;
} // End of detect function
