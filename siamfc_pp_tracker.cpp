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

SiamFCPPTracker2::SiamFCPPTracker2(const std::string& onnxPath ,const std::string& tonnxPath) {
    std::cout << "[SiamFC_PP] Constructor called with model path: " << onnxPath << std::endl;
    std::cout << "[SiamFC_PP] Constructor called with tracker model path: " << tonnxPath << std::endl;

    // Initialize hyperparameters
    _hyper_params["score_size"] = 17; // Adjust based on model
    _hyper_params["windowing"] = std::string("cosine");

    // Initialize the neural network
    try {
        std::cout << "[SiamFC_PP] Loading ONNX model from: " << onnxPath << std::endl;
        net_ = cv::dnn::readNetFromONNX(onnxPath);
        tnet_ = cv::dnn::readNetFromONNX(tonnxPath);
        // Try to use most compatible backend/target
        try {
            // Try OpenCV backend first
            std::cout << "[SiamFC_PP] Trying OpenCV backend..." << std::endl;
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            tnet_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            tnet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        } catch (const cv::Exception& e) {
            std::cerr << "[SiamFC_PP WARNING] Failed to set OpenCV backend: " << e.what() << std::endl;
            // Fallback to default
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            tnet_.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
            tnet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        }

        std::cout << "[SiamFC_PP] SiamFC_PP tracker initialized successfully with model: " << onnxPath << std::endl;
    } catch (const cv::Exception& e) {
        std::cerr << "[SiamFC_PP ERROR] Failed to load ONNX model: " << e.what() << std::endl;
        throw std::runtime_error("Failed to load ONNX model at " + onnxPath + ": " + std::string(e.what()));
    }
}

cv::Mat createRandomBlob(const std::vector<int>& shape, float minVal = 0.0f, float maxVal = 1.0f) {
    if (shape.size() != 4) {
        throw std::runtime_error("Shape must have 4 dimensions [N, C, H, W]");
    }

    // Create a 4D Mat with the specified shape
    cv::Mat blob(shape, CV_32F);

    // Fill with random values
    cv::RNG rng(cv::getTickCount()); // Seed with current time for randomness
    rng.fill(blob, cv::RNG::UNIFORM, minVal, maxVal);

    // Ensure the blob is continuous for DNN compatibility
    if (!blob.isContinuous()) {
        blob = blob.clone();
    }

    return blob;
}

void SiamFCPPTracker2::init(const cv::Mat& frame, const std::vector<double>& bbox) {
    //aligned box
    std::vector<float> aligned_bbox;
    double cx, cy, w, h;
    std::tie(cx, cy, w, h) = get_axis_aligned_bbox(bbox);
    aligned_bbox = {static_cast<float>(cx), static_cast<float>(cy),
                    static_cast<float>(w), static_cast<float>(h)};

    //gt box
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

        //        // Convert to float32 and normalize to 0-1 range
        //        cv::Mat float_img;
        //        im_z_crop.convertTo(float_img, CV_32F, 1.0/255.0);

        cv::Mat blob = cv::dnn::blobFromImage(
                    im_z_crop,              // Input image
                    1.0,                    // Scalefactor
                    cv::Size(),             // Use original size
                    cv::Scalar(0.485, 0.456, 0.406),  // ImageNet mean values
                    false,                  // swapRB: keep as BGR
                    false                   // crop: no cropping
                    );

        net_.setInput(blob, "im_z");
        std::vector<cv::Mat> outputs;
        net_.forward(outputs, outNames);
        _state["c_x"] = outputs[c_x_idx];
        _state["r_x"] = outputs[r_x_idx];
        _state["target_pos"] = target_pos;
        _state["target_sz"] = target_sz;
//        postprocessScore();
        //TODO: saving the z_crop image, window, and avg_chans


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

    //gettign prior information
    auto target_pos_prior = std::any_cast<std::vector<float>>(_state["target_pos"]);
    auto target_sz_prior = std::any_cast<std::vector<float>>(_state["target_sz"]);
    auto avg_chans = std::any_cast<std::vector<float>>(_state["avg_chans"]);
    auto c_x_ = _state["c_x"]; // Adjust if this needs casting
    auto r_x_ = _state["r_x"]; // Adjust if this needs casting;

    //set hyperparameteres
    int z_size = 127;
    int x_size = 303;

    cv::Mat im_x_crop;

    //get crop related function
    float scale;
    auto func_get_subwindow = [this](const cv::Mat& im, const std::vector<float>& pos, int model_sz, int original_sz, const std::vector<float>& avg_chans) {
        return this->get_subwindow_tracking(im, pos, model_sz, original_sz, avg_chans);
    };
    std::tie(im_x_crop, scale) = get_crop(
                frame, target_pos_prior, target_sz_prior, z_size, x_size, avg_chans, 0.5, func_get_subwindow, -1, cv::Mat()
                );

    if (im_x_crop.empty()) {
        std::cerr << "[SiamFC_PP ERROR] Empty crop returned" << std::endl;
        throw std::runtime_error("Failed to create crop");
    }

    _state["scale_x"] = scale;

    //input of the model
    cv::Mat blob = cv::dnn::blobFromImage(
                im_x_crop,              // Input image
                1.0,                    // Scalefactor
                cv::Size(),             // Use original size
                cv::Scalar(0.485, 0.456, 0.406),  // ImageNet mean values
                false,                  // swapRB: keep as BGR
                false                   // crop: no cropping
                );

    std::vector<std::string> outNames = {"score", "bbox"};

    net_.setInput(blob, "im_x");
    std::vector<cv::Mat> outputs;
    net_.forward(outputs, outNames);

    // Retrieve outputs
    cv::Mat score = outputs[0]; // Shape: [1, 289, 1], float32
    cv::Mat bbox = outputs[1];  // Shape: [1, 289, 4], float32
//    cv::Mat box_wh = xyxy2cxywh(bbox);

    //postprocess related variables
//    int best_pscore_id;
//    std::vector<float> pscore;
//    std::vector<float> penalty;
//    std::vector<float> window;
//    float penalty_k;
//    float window_influence;
//    float test_lr;

    //postprocess
//    best_pscore_id,pscore,penalty = postprocessscore(score, box_wh, target_sz_prior, scale, window, penalty_k, window_influence);
//    new_target_pos, new_target_sz = postprocessBox(best_pscore_id, pscore, box_wh, target_pos_prior, target_sz_prior, scale, x_size, penalty, test_lr);
//    restricted_pos, restricted_sz = restrictBox(new_target_pos, new_target_sz, frame.cols, frame.rows, 0.0f, 0.0f);
    //    out, score
    //    pred_bbox = [out[0], out[1], out[0] + out[2], out[1]+out[3]]
}

std::tuple<int, std::vector<float>, std::vector<float>>
SiamFCPPTracker2::postprocessScore(const std::vector<float>& score,
                 const std::vector<float>& box_wh,
                 const cv::Size2f& target_sz,
                 float scale_x,
                 const std::vector<float>& window,
                 float penalty_k,
                 float window_influence) {

    // Define helper functions
    auto change = [](float r) -> float {
        return std::max(r, 1.0f / r);
    };

    auto sz = [](float w, float h) -> float {
        float pad = (w + h) * 0.5f;
        float sz2 = (w + pad) * (h + pad);
        return std::sqrt(sz2);
    };

    auto sz_wh = [](const cv::Size2f& wh) -> float {
        float pad = (wh.width + wh.height) * 0.5f;
        float sz2 = (wh.width + pad) * (wh.height + pad);
        return std::sqrt(sz2);
    };

    // Size of score vector (HW)
    size_t score_size = score.size();

    // Target size in crop coordinates
    cv::Size2f target_sz_in_crop(target_sz.width * scale_x, target_sz.height * scale_x);

    // Compute scale and ratio penalties for each candidate
    std::vector<float> penalty(score_size);
    std::vector<float> s_c(score_size);
    std::vector<float> r_c(score_size);

    // Target aspect ratio
    float target_aspect_ratio = target_sz_in_crop.width / target_sz_in_crop.height;

    // Calculate sz_wh of target size once
    float target_sz_wh = sz_wh(target_sz_in_crop);

    // Calculate penalties for each candidate
    for (size_t i = 0; i < score_size; i++) {
        // Each box has 4 values (cx, cy, w, h), so we index with i*4+2 for width and i*4+3 for height
        float w = box_wh[i * 4 + 2];
        float h = box_wh[i * 4 + 3];

        // Scale penalty
        s_c[i] = change(sz(w, h) / target_sz_wh);

        // Ratio/aspect penalty
        r_c[i] = change(target_aspect_ratio / (w / h));

        // Combined penalty
        penalty[i] = std::exp(-(r_c[i] * s_c[i] - 1) * penalty_k);
    }

    // Apply penalty to score
    std::vector<float> pscore(score_size);
    for (size_t i = 0; i < score_size; i++) {
        pscore[i] = penalty[i] * score[i];
    }

    // Apply cosine window (motion model)
    for (size_t i = 0; i < score_size; i++) {
        pscore[i] = pscore[i] * (1 - window_influence) + window[i] * window_influence;
    }

    // Find the best score
    auto best_pscore_it = std::max_element(pscore.begin(), pscore.end());
    int best_pscore_id = std::distance(pscore.begin(), best_pscore_it);

    return {best_pscore_id, pscore, penalty};
}

std::pair<cv::Point2f, cv::Size2f>
SiamFCPPTracker2::postprocessBox(int best_pscore_id,
               const std::vector<float>& score,
               const std::vector<float>& box_wh,
               const cv::Point2f& target_pos,
               const cv::Size2f& target_sz,
               float scale_x,
               int x_size,
               const std::vector<float>& penalty,
               float test_lr) {

    // Extract the best box
    float pred_cx = box_wh[best_pscore_id * 4 + 0] / scale_x;
    float pred_cy = box_wh[best_pscore_id * 4 + 1] / scale_x;
    float pred_w = box_wh[best_pscore_id * 4 + 2] / scale_x;
    float pred_h = box_wh[best_pscore_id * 4 + 3] / scale_x;

    // Calculate learning rate based on penalty and score
    float lr = penalty[best_pscore_id] * score[best_pscore_id] * test_lr;

    // Calculate new position (add offset)
    float half_patch_size = static_cast<float>(x_size) / 2.0f;
    float res_x = pred_cx + target_pos.x - half_patch_size / scale_x;
    float res_y = pred_cy + target_pos.y - half_patch_size / scale_x;

    // Update size with learning rate
    float res_w = target_sz.width * (1.0f - lr) + pred_w * lr;
    float res_h = target_sz.height * (1.0f - lr) + pred_h * lr;

    // Create new target position and size
    cv::Point2f new_target_pos(res_x, res_y);
    cv::Size2f new_target_sz(res_w, res_h);

    return {new_target_pos, new_target_sz};
}

cv::Mat SiamFCPPTracker2::xyxy2cxywh(const cv::Mat& bbox) {
    // Validate input shape: expecting [1, 289, 4]
    if (bbox.dims != 3 || bbox.size[0] != 1 || bbox.size[1] != 289 || bbox.size[2] != 4) {
        throw std::invalid_argument("Input cv::Mat must have shape [1, 289, 4]");
    }

    // Initialize output matrix: shape [289, 4], type CV_32F (float)
    cv::Mat result(289, 4, CV_32F);

    // Process each bounding box
    for (int i = 0; i < 289; ++i) {
        // Access the [x1, y1, x2, y2] for the i-th box
        float x1 = bbox.at<float>(0, i, 0);
        float y1 = bbox.at<float>(0, i, 1);
        float x2 = bbox.at<float>(0, i, 2);
        float y2 = bbox.at<float>(0, i, 3);

        // Compute [cx, cy, w, h]
        result.at<float>(i, 0) = (x1 + x2) / 2.0f; // cx = (x1 + x2) / 2
        result.at<float>(i, 1) = (y1 + y2) / 2.0f; // cy = (y1 + y2) / 2
        result.at<float>(i, 2) = x2 - x1 + 1.0f;  // w = x2 - x1 + 1
        result.at<float>(i, 3) = y2 - y1 + 1.0f;  // h = y2 - y1 + 1
    }

    return result;
}

std::pair<cv::Point2f, cv::Size2f>
SiamFCPPTracker2::restrictBox(const cv::Point2f& target_pos,
            const cv::Size2f& target_sz,
            int im_w,
            int im_h,
            float min_w,
            float min_h) {

    // Create copies that will be modified
    cv::Point2f restricted_pos = target_pos;
    cv::Size2f restricted_sz = target_sz;

    // Restrict position to image boundaries
    restricted_pos.x = std::max(0.0f, std::min(static_cast<float>(im_w), target_pos.x));
    restricted_pos.y = std::max(0.0f, std::min(static_cast<float>(im_h), target_pos.y));

    // Restrict size to be within valid range
    restricted_sz.width = std::max(min_w, std::min(static_cast<float>(im_w), target_sz.width));
    restricted_sz.height = std::max(min_h, std::min(static_cast<float>(im_h), target_sz.height));

    return {restricted_pos, restricted_sz};
}
