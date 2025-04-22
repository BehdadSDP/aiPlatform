#include "siamfc_pp_tracker.h"
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <tuple>

SiamFCPPTracker2::SiamFCPPTracker2(const std::string& onnxPath) {
    // Initialize hyperparameters
    _hyper_params["score_size"] = 17; // Adjust based on model
    _hyper_params["windowing"] = std::string("cosine");

    // Initialize the neural network
    try {
        net_ = cv::dnn::readNetFromONNX(onnxPath);
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        std::cout << "SiamFC_PP tracker initialized with model: " << onnxPath << std::endl;
    } catch (const cv::Exception& e) {
        throw std::runtime_error("Failed to load ONNX model at " + onnxPath + ": " + std::string(e.what()));
    }
}

void SiamFCPPTracker2::init(const cv::Mat& frame, const std::vector<double>& bbox) {
    std::vector<float> aligned_bbox;
    double cx, cy, w, h;
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

    int z_size = 4;
    float context_amount = 0.5;

    cv::Mat im_z_crop;
    float scale;
    auto func_get_subwindow = [this](const cv::Mat& im, const std::vector<float>& pos, int model_sz, int original_sz, const std::vector<float>& avg_chans) {
        return this->get_subwindow_tracking(im, pos, model_sz, original_sz, avg_chans);
    };
    std::tie(im_z_crop, scale) = get_crop(
        frame, target_pos, target_sz, z_size, -1, avg_chans, context_amount, func_get_subwindow, -1, cv::Mat()
    );

    // Extract template feature
    cv::Mat c_x, r_x;
    std::tie(c_x, r_x) = feature(im_z_crop);

    // Store features
    _state["c_x"] = c_x;
    _state["r_x"] = r_x;

    int score_size = std::any_cast<int>(_hyper_params["score_size"]);
    std::vector<float> window;

    if (std::any_cast<std::string>(_hyper_params["windowing"]) == "cosine") {
        std::vector<float> hanning_x = createHanning(score_size);
        std::vector<float> hanning_y = createHanning(score_size);
        window.resize(score_size * score_size);
        for (int i = 0; i < score_size; i++) {
            for (int j = 0; j < score_size; j++) {
                window[i * score_size + j] = hanning_x[i] * hanning_y[j];
            }
        }
    } else {
        window.resize(score_size * score_size, 1.0f);
    }

    _state["z_crop"] = im_z_crop;
    _state["avg_chans"] = avg_chans;
    _state["window"] = window;
    _state["state"] = std::make_pair(target_pos, target_sz);
}

std::tuple<cv::Mat, cv::Mat> SiamFCPPTracker2::feature(const cv::Mat& im) {
    cv::Mat blob = cv::dnn::blobFromImage(im);
    net_.setInput(blob);

    std::vector<cv::Mat> outputs;
    net_.forward(outputs, net_.getUnconnectedOutLayersNames());

    if (outputs.size() < 6) {
        throw std::runtime_error("Insufficient outputs from neural network in feature method");
    }

    cv::Mat c_x = outputs[4];
    cv::Mat r_x = outputs[5];

    _state["net"] = net_;
    _state["c_x"] = c_x;
    _state["r_x"] = r_x;

    return std::make_tuple(c_x, r_x);
}

std::vector<float> SiamFCPPTracker2::createHanning(int size) {
    std::vector<float> window(size);
    for (int i = 0; i < size; i++) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
    }
    return window;
}

std::tuple<double, double, double, double> SiamFCPPTracker2::get_axis_aligned_bbox(const std::vector<double>& region) {
    int nv = region.size();
    double cx, cy, w, h;

    if (nv == 8) {
        std::vector<double> x_coords;
        std::vector<double> y_coords;
        for (int i = 0; i < nv; i += 2) {
            x_coords.push_back(region[i]);
            y_coords.push_back(region[i+1]);
        }

        cx = std::accumulate(x_coords.begin(), x_coords.end(), 0.0) / (nv/2);
        cy = std::accumulate(y_coords.begin(), y_coords.end(), 0.0) / (nv/2);

        double x1 = *std::min_element(x_coords.begin(), x_coords.end());
        double x2 = *std::max_element(x_coords.begin(), x_coords.end());
        double y1 = *std::min_element(y_coords.begin(), y_coords.end());
        double y2 = *std::max_element(y_coords.begin(), y_coords.end());

        double norm1 = std::sqrt(std::pow(region[0] - region[2], 2) + std::pow(region[1] - region[3], 2));
        double norm2 = std::sqrt(std::pow(region[2] - region[4], 2) + std::pow(region[3] - region[5], 2));
        double A1 = norm1 * norm2;
        double A2 = (x2 - x1) * (y2 - y1);
        double s = std::sqrt(A1 / A2);

        w = s * (x2 - x1) + 1;
        h = s * (y2 - y1) + 1;
    } else {
        double x = region[0];
        double y = region[1];
        w = region[2];
        h = region[3];
        cx = x + w/2;
        cy = y + h/2;
    }

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

    if (x_size == -1) {
        x_size = z_size;
    }
    s_crop = x_size / scale;

    if (output_size == -1) {
        output_size = x_size;
    }

    cv::Mat im_crop;
    if (!mask.empty()) {
        cv::Mat mask_crop;
        im_crop = func_get_subwindow(im, target_pos, output_size, std::round(s_crop), avg_chans);
        return {im_crop, scale};
    } else {
        im_crop = func_get_subwindow(im, target_pos, output_size, std::round(s_crop), avg_chans);
        return {im_crop, scale};
    }
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
    cv::warpAffine(im,
                   im_patch,
                   mat2x3,
                   cv::Size(model_sz, model_sz),
                   cv::INTER_LINEAR | cv::WARP_INVERSE_MAP,
                   cv::BORDER_CONSTANT,
                   borderValue);

    if (!mask.empty()) {
        cv::Mat mask_patch;
        cv::warpAffine(mask,
                       mask_patch,
                       mat2x3,
                       cv::Size(model_sz, model_sz),
                       cv::INTER_NEAREST | cv::WARP_INVERSE_MAP);
        return im_patch;
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
    return _state.find("state") != _state.end();
}

float SiamFCPPTracker2::getTrackingScore() const {
    return _state.find("state") != _state.end() ? 1.0f : 0.0f;
}

cv::Rect SiamFCPPTracker2::update(const cv::Mat& frame) {

}
