#include "siamfc_pp_tracker.h"
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <iomanip> // For formatting output
#include <fstream>

bool validateBlob(const cv::Mat& blob, const std::string& debug_name = "blob") {
    // Check if blob is empty
    if (blob.empty()) {
        std::cerr << "[Validation ERROR] " << debug_name << " is empty" << std::endl;
        return false;
    }

    // Check dimensions
    if (blob.dims != 4) {
        std::cerr << "[Validation ERROR] " << debug_name << " dimensions should be 4, got " << blob.dims << std::endl;
        return false;
    }

    // Check if size is reasonable
    if (blob.size[0] != 1) {
        std::cerr << "[Validation ERROR] " << debug_name << " batch size should be 1, got " << blob.size[0] << std::endl;
        return false;
    }

    if (blob.size[1] != 3) {
        std::cerr << "[Validation ERROR] " << debug_name << " channels should be 3, got " << blob.size[1] << std::endl;
        return false;
    }

    // Check for NaN or Inf values
    bool hasNaN = false;
    bool hasInf = false;
    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();
    
    // Get pointer to data
    float* data = (float*)blob.data;
    size_t total = blob.total() * blob.channels();
    
    for (size_t i = 0; i < total; i++) {
        if (std::isnan(data[i])) hasNaN = true;
        if (std::isinf(data[i])) hasInf = true;
        minVal = std::min(minVal, (double)data[i]);
        maxVal = std::max(maxVal, (double)data[i]);
    }
    
    if (hasNaN) {
        std::cerr << "[Validation ERROR] " << debug_name << " contains NaN values" << std::endl;
        return false;
    }
    
    if (hasInf) {
        std::cerr << "[Validation ERROR] " << debug_name << " contains Inf values" << std::endl;
        return false;
    }
    
    // Print value range
    std::cout << "[Validation] " << debug_name << " value range: [" << minVal << ", " << maxVal << "]" << std::endl;
    
    // Dump first few values for inspection
    std::cout << "[Validation] " << debug_name << " first 10 values: ";
    for (int i = 0; i < std::min(10, (int)total); i++) {
        std::cout << data[i] << " ";
    }
    std::cout << std::endl;
    
    // Save blob to file for inspection
    try {
        std::string filename = debug_name + "_dump.bin";
        std::ofstream file(filename, std::ios::binary);
        if (file.is_open()) {
            file.write((char*)blob.data, total * sizeof(float));
            file.close();
            std::cout << "[Validation] Saved " << debug_name << " to " << filename << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Validation WARNING] Failed to save blob to file: " << e.what() << std::endl;
    }
    
    return true;
}

SiamFCPPTracker2::SiamFCPPTracker2(const std::string& onnxPath) {
    std::cout << "[SiamFC_PP] Constructor called with model path: " << onnxPath << std::endl;

    // Initialize hyperparameters
    _hyper_params["score_size"] = 17; // Adjust based on model
    _hyper_params["windowing"] = std::string("cosine");
    std::cout << "[SiamFC_PP] Hyperparameters initialized: score_size=" << std::any_cast<int>(_hyper_params["score_size"])
            << ", windowing=" << std::any_cast<std::string>(_hyper_params["windowing"]) << std::endl;

    // Initialize the neural network
    try {
        std::cout << "[SiamFC_PP] Loading ONNX model from: " << onnxPath << std::endl;
        net_ = cv::dnn::readNetFromONNX(onnxPath);
        
        // List available backends
        std::cout << "[SiamFC_PP] Available DNN backends:" << std::endl;
        if (cv::dnn::getAvailableBackends().empty()) {
            std::cout << "  - None detected, using default" << std::endl;
        } else {
            for (auto& backend : cv::dnn::getAvailableBackends()) {
                std::cout << "  - Backend: " << backend.first << ", Target: " << backend.second << std::endl;
            }
        }
        
        // Try to use most compatible backend/target
        try {
            // Try OpenCV backend first
            std::cout << "[SiamFC_PP] Trying OpenCV backend..." << std::endl;
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        } catch (const cv::Exception& e) {
            std::cerr << "[SiamFC_PP WARNING] Failed to set OpenCV backend: " << e.what() << std::endl;
            // Fallback to default
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        }

        std::cout << "[SiamFC_PP] SiamFC_PP tracker initialized successfully with model: " << onnxPath << std::endl;
    } catch (const cv::Exception& e) {
        std::cerr << "[SiamFC_PP ERROR] Failed to load ONNX model: " << e.what() << std::endl;
        throw std::runtime_error("Failed to load ONNX model at " + onnxPath + ": " + std::string(e.what()));
    }
}

void SiamFCPPTracker2::init(const cv::Mat& frame, const std::vector<double>& bbox) {
    std::vector<float> aligned_bbox;
    double cx, cy, w, h;
    std::cout << "bbox size: " << bbox.size() << std::endl;
    std::tie(cx, cy, w, h) = get_axis_aligned_bbox(bbox);
    aligned_bbox = {static_cast<float>(cx), static_cast<float>(cy),
                    static_cast<float>(w), static_cast<float>(h)};

    std::vector<float> gt_bbox = {
        aligned_bbox[0] - (aligned_bbox[2] - 1) / 2.0f,
        aligned_bbox[1] - (aligned_bbox[3] - 1) / 2.0f,
        aligned_bbox[2],
        aligned_bbox[3]
    };

    std::vector<float> rect = gt_bbox;
    std::vector<float> box = xywh2cxywh(rect);
    std::vector<float> target_pos = {box[0], box[1]};
    std::vector<float> target_sz = {box[2], box[3]};

    _state["im_h"] = frame.rows;
    _state["im_w"] = frame.cols;

    // Calculate average channel values from the frame
    std::vector<float> avg_chans(3, 0.0f);
    for (int c = 0; c < 3; c++) {
        float sum = 0.0f;
        int count = 0;
        for (int i = 0; i < frame.rows; i++) {
            for (int j = 0; j < frame.cols; j++) {
                sum += frame.at<cv::Vec3b>(i, j)[c];
                count++;
            }
        }
        avg_chans[c] = sum / count;
    }

    // Use only 127x127 input size for the model
    int z_size = 127;
    float context_amount = 0.5;

    cv::Mat im_z_crop;
    float scale;
    auto func_get_subwindow = [this](const cv::Mat& im, const std::vector<float>& pos, int model_sz, int original_sz, const std::vector<float>& avg_chans) {
        return this->get_subwindow_tracking(im, pos, model_sz, original_sz, avg_chans);
    };
    std::tie(im_z_crop, scale) = get_crop(
                frame, target_pos, target_sz, z_size, -1, avg_chans, context_amount, func_get_subwindow, -1, cv::Mat()
                );
    
    if (im_z_crop.empty()) {
        std::cerr << "[SiamFC_PP ERROR] Empty crop returned" << std::endl;
        throw std::runtime_error("Failed to create crop");
    }
    
    // Extract template feature
    cv::Mat c_x, r_x;
    try {
        std::vector<std::string> outNames = net_.getUnconnectedOutLayersNames();
        // Find the indices for "c_x" and "r_x" in the outNames vector
        int c_x_idx = -1;
        int r_x_idx = -1;
        for (int i = 0; i < outNames.size(); i++) {
            if (outNames[i] == "c_z_k") c_x_idx = i;
            if (outNames[i] == "r_z_k") r_x_idx = i;
        }

        // Display image dimensions before blob creation
        std::cout << "[SiamFC_PP DEBUG] Crop dimensions: " << im_z_crop.size()
                  << ", channels: " << im_z_crop.channels()
                  << ", type: " << im_z_crop.type() << std::endl;

        // Convert to float32 and normalize to 0-1 range
        cv::Mat float_img;
        im_z_crop.convertTo(float_img, CV_32F, 1.0/255.0);

        cv::Mat blob = cv::dnn::blobFromImage(
                    float_img,              // Input image
                    1.0,                    // Scalefactor
                    cv::Size(),             // Use original size
                    cv::Scalar(0.485, 0.456, 0.406),  // ImageNet mean values
                    false,                  // swapRB: keep as BGR
                    false                   // crop: no cropping
                    );
        
        // Validate the blob before feeding to network
        std::string blob_name = "z_blob";
        if (!validateBlob(blob, blob_name)) {
            std::cerr << "[SiamFC_PP ERROR] Invalid blob" << std::endl;
            throw std::runtime_error("Invalid blob");
        }

        net_.setInput(blob);
        std::vector<cv::Mat> outputs;
        
        // Print output names for debugging
        for (const auto& name : outNames) {
            std::cout << name << " ";
        }
        std::cout << std::endl;

        net_.forward(outputs, outNames);
        _state["c_x"] = outputs[c_x_idx];
        _state["r_x"] = outputs[r_x_idx];

        // Print shape information
        cv::Mat c_x = outputs[c_x_idx];
        cv::Mat r_x = outputs[r_x_idx];
        std::cout << "c_x shape: [" << c_x.size[0] << ", " << c_x.size[1] << ", " << c_x.size[2] << "]" << std::endl;
        std::cout << "r_x shape: [" << r_x.size[0] << ", " << r_x.size[1] << ", " << r_x.size[2] << "]" << std::endl;

    } catch (const cv::Exception& e) {
        std::cerr << "[SiamFC_PP ERROR] OpenCV exception in feature extraction: " << e.what() << std::endl;
        throw;
    } catch (const std::exception& e) {
        std::cerr << "[SiamFC_PP ERROR] Exception in feature extraction: " << e.what() << std::endl;
        throw;
    }
}

std::vector<float> SiamFCPPTracker2::createHanning(int size) {
    std::cout << "[SiamFC_PP] Creating Hanning window of size: " << size << std::endl;

    std::vector<float> window(size);
    for (int i = 0; i < size; i++) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
    }
    return window;
}

std::tuple<double, double, double, double> SiamFCPPTracker2::get_axis_aligned_bbox(const std::vector<double>& region) {
    double cx, cy, w, h;
    double x = region[0];
    double y = region[1];
    w = region[2];
    h = region[3];
    cx = x + w/2;
    cy = y + h/2;
    return std::make_tuple(cx, cy, w, h);
}

std::vector<float> SiamFCPPTracker2::xywh2cxywh(const std::vector<float>& rect) {
    std::vector<float> result(4);
    result[0] = rect[0] + (rect[2] - 1) / 2.0f;
    result[1] = rect[1] + (rect[3] - 1) / 2.0f;
    result[2] = rect[2];
    result[3] = rect[3];
    return result;
}

std::tuple<cv::Mat, float> SiamFCPPTracker2::get_crop(
        const cv::Mat& im,
        const std::vector<float>& target_pos,
        const std::vector<float>& target_sz,
        int z_size,
        int x_size,
        const std::vector<float>& avg_chans,
        float context_amount,
        std::function<cv::Mat(const cv::Mat&, const std::vector<float>&, int, int, const std::vector<float>&)> func_get_subwindow,
        int output_size,
        const cv::Mat& mask) {

    float wc = target_sz[0] + context_amount * (target_sz[0] + target_sz[1]);
    float hc = target_sz[1] + context_amount * (target_sz[0] + target_sz[1]);
    float s_crop = std::sqrt(wc * hc);
    float scale = z_size / s_crop;

    // If x_size is default (-1), use z_size
    int actual_x_size = (x_size <= 0) ? z_size : x_size;
    
    // Calculate crop size
    s_crop = (x_size <= 0) ? s_crop : (actual_x_size / scale);
    
    // Use model_sz as output size if not specified
    int actual_output_size = (output_size <= 0) ? actual_x_size : output_size;
    
    cv::Mat im_crop;
    // Get the subwindow
    try {
        if (func_get_subwindow) {
            im_crop = func_get_subwindow(im, target_pos, actual_output_size, std::round(s_crop), avg_chans);
            // Verify crop was created successfully
            if (im_crop.empty()) {
                std::cerr << "[SiamFC_PP WARNING] Empty crop returned from get_subwindow_tracking" << std::endl;
            } else {
                std::cout << "[SiamFC_PP] Crop created with size: " << im_crop.size() << std::endl;
            }
        } else {
            std::cerr << "[SiamFC_PP ERROR] No subwindow function provided" << std::endl;
            // Create a default black image as fallback
            im_crop = cv::Mat::zeros(actual_output_size, actual_output_size, im.type());
        }
    } catch (const cv::Exception& e) {
        std::cerr << "[SiamFC_PP ERROR] Exception in get_crop: " << e.what() << std::endl;
        // Create a default black image as fallback
        im_crop = cv::Mat::zeros(actual_output_size, actual_output_size, im.type());
    }
    
    return {im_crop, scale};
}

cv::Mat SiamFCPPTracker2::get_subwindow_tracking(
        const cv::Mat& im,
        const std::vector<float>& pos,
        int model_sz,
        int original_sz,
        const std::vector<float>& avg_chans,
        const cv::Mat& mask) {

    std::vector<float> crop_cxywh = {
        pos[0],
        pos[1],
        static_cast<float>(original_sz),
        static_cast<float>(original_sz)
    };

    std::vector<float> crop_xyxy = cxywh2xyxy(crop_cxywh);
    float M_13 = crop_xyxy[0];
    float M_23 = crop_xyxy[1];
    float M_11 = (crop_xyxy[2] - M_13) / (model_sz - 1);
    float M_22 = (crop_xyxy[3] - M_23) / (model_sz - 1);

    cv::Mat mat2x3 = (cv::Mat_<float>(2, 3) <<
                      M_11, 0, M_13,
                      0, M_22, M_23);

    cv::Scalar borderValue(avg_chans[0], avg_chans[1], avg_chans[2]);
    cv::Mat im_patch;
    try {
        cv::warpAffine(im,
                       im_patch,
                       mat2x3,
                       cv::Size(model_sz, model_sz),
                       cv::INTER_LINEAR | cv::WARP_INVERSE_MAP,
                       cv::BORDER_CONSTANT,
                       borderValue);
        std::cout << "[SiamFC_PP] Warped patch created with size: " << im_patch.size() << std::endl;
    } catch (const cv::Exception& e) {
        std::cerr << "[SiamFC_PP ERROR] warpAffine failed: " << e.what() << std::endl;
        throw;
    }

    if (!mask.empty()) {
        cv::Mat mask_patch;
        cv::warpAffine(mask,
                       mask_patch,
                       mat2x3,
                       cv::Size(model_sz, model_sz),
                       cv::INTER_NEAREST | cv::WARP_INVERSE_MAP);
    }

    return im_patch;
}

std::vector<float> SiamFCPPTracker2::cxywh2xyxy(const std::vector<float>& box) {
    std::vector<float> xyxy(4);
    xyxy[0] = box[0] - (box[2] - 1) / 2.0f;
    xyxy[1] = box[1] - (box[3] - 1) / 2.0f;
    xyxy[2] = box[0] + (box[2] - 1) / 2.0f;
    xyxy[3] = box[1] + (box[3] - 1) / 2.0f;
    return xyxy;
}

bool SiamFCPPTracker2::isInitialized() const {
    bool initialized = _state.find("state") != _state.end();
    return initialized;
}

float SiamFCPPTracker2::getTrackingScore() const {
    float score = _state.find("state") != _state.end() ? 1.0f : 0.0f;
    return score;
}

cv::Rect SiamFCPPTracker2::update(const cv::Mat& frame) {
    if (!isInitialized()) {
        std::cerr << "[SiamFC_PP ERROR] Tracker not initialized" << std::endl;
        return cv::Rect();
    }
    
    try {
        // Get the successful z_size from initialization
        int z_size = 127;  // Default
        if (_state.find("z_size") != _state.end()) {
            z_size = std::any_cast<int>(_state["z_size"]);
        }
        
        // Get the stored values from state
        std::vector<float> target_pos = std::any_cast<std::vector<float>>(_state["target_pos"]);
        std::vector<float> target_sz = std::any_cast<std::vector<float>>(_state["target_sz"]);
        std::vector<float> avg_chans = std::any_cast<std::vector<float>>(_state["avg_chans"]);
        float context_amount = std::any_cast<float>(_state["context_amount"]);
        
        // Use larger search area for detection (typically around 255)
        int x_size = z_size * 2;  // Common practice is to use larger size for search
        
        // Extract search region
        cv::Mat im_x_crop;
        float scale;
        auto func_get_subwindow = [this](const cv::Mat& im, const std::vector<float>& pos, int model_sz, int original_sz, const std::vector<float>& avg_chans) {
            return this->get_subwindow_tracking(im, pos, model_sz, original_sz, avg_chans);
        };
        
        try {
            std::tie(im_x_crop, scale) = get_crop(
                        frame, target_pos, target_sz, x_size, -1, avg_chans, context_amount, func_get_subwindow, -1, cv::Mat()
                        );
            
            if (im_x_crop.empty()) {
                std::cerr << "[SiamFC_PP ERROR] Empty crop returned for search region" << std::endl;
                return cv::Rect();
            }
            
            // In a real implementation, we would process the search region through the network,
            // and update the target position. For now, just return a placeholder rect.
            
            // Return last known position as rectangle
            int x = static_cast<int>(target_pos[0] - target_sz[0]/2);
            int y = static_cast<int>(target_pos[1] - target_sz[1]/2);
            int width = static_cast<int>(target_sz[0]);
            int height = static_cast<int>(target_sz[1]);
            
            return cv::Rect(x, y, width, height);
        } catch (const std::exception& e) {
            std::cerr << "[SiamFC_PP ERROR] Exception in get_crop: " << e.what() << std::endl;
            return cv::Rect();
        }
    } catch (const std::exception& e) {
        std::cerr << "[SiamFC_PP ERROR] Exception in update: " << e.what() << std::endl;
        return cv::Rect();
    }
}

