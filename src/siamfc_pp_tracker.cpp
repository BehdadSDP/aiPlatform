#include "include/siamfc_pp_tracker.h"
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <iomanip>
#include <fstream>
#include <chrono>

struct MinMaxValues {
    float min_val;
    float max_val;
};

MinMaxValues calculateMinMax(const cv::Mat& tensor) {
    MinMaxValues result;
    result.min_val = std::numeric_limits<float>::max();
    result.max_val = std::numeric_limits<float>::lowest();
    
    if (tensor.empty()) {
        return result;
    }
    
    const float* data = (float*)tensor.data;
    size_t total_elements = tensor.total() * tensor.channels();
    for (size_t i = 0; i < total_elements; i++) {
        result.min_val = std::min(result.min_val, data[i]);
        result.max_val = std::max(result.max_val, data[i]);
    }
    
    return result;
}

float calculateScalePenalty(const cv::Size2f& target_sz, const cv::Size2f& candidate_sz) {
    float w_ratio = candidate_sz.width / target_sz.width;
    float h_ratio = candidate_sz.height / target_sz.height;
    float scale_change = (w_ratio * h_ratio) - 1.0f;
    float penalty_k = 0.04f;
    return std::exp(-scale_change * scale_change / penalty_k);
}

SiamFCPPTracker2::SiamFCPPTracker2() {
    z_size_ = 127;
    x_size_ = 303;
    context_amount_ = 0.5f;
    score_size_ = 17;
    stride_ = 8;
    penalty_k_ = 0.04f;
    window_influence_ = 0.21f;
    test_lr_ = 0.52f;
    min_w_ = 10.0f;
    min_h_ = 10.0f;

    initCosineWindow();
    template_features_.resize(2);
    is_initialized_ = false;
}

SiamFCPPTracker2::~SiamFCPPTracker2() = default;

bool SiamFCPPTracker2::loadModel(const std::string& feature_model_path, const std::string& track_model_path) {
    try {
        feature_net_ = cv::dnn::readNetFromONNX(feature_model_path);
        if (feature_net_.empty()) {
            return false;
        }

        track_net_ = cv::dnn::readNetFromONNX(track_model_path);
        if (track_net_.empty()) {
            return false;
        }

        feature_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        feature_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        track_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        track_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        return true;
    } catch (const cv::Exception& e) {
        return false;
    } catch (const std::exception& e) {
        return false;
    }
}

bool SiamFCPPTracker2::init(const cv::Mat& frame, const cv::Rect& bbox) {
    try {
        cv::Rect adjBbox = bbox;
        adjBbox.y = int(bbox.y + bbox.height);
        adjBbox.x = adjBbox.x + (adjBbox.width/2);
        adjBbox.y = adjBbox.y - (adjBbox.height/2);
        adjBbox.x = adjBbox.x - ((adjBbox.width - 1)/2);
        adjBbox.y = adjBbox.y + ((adjBbox.height - 1)/2);
        adjBbox = xywhToCxywh(adjBbox);

        im_w_ = frame.cols;
        im_h_ = frame.rows;

        cv::Rect2f rect_f(static_cast<float>(adjBbox.x),
                          static_cast<float>(adjBbox.y),
                          static_cast<float>(adjBbox.width),
                          static_cast<float>(adjBbox.height));

        target_pos_ = cv::Point2f(rect_f.x, rect_f.y);
        target_sz_ = cv::Size2f(rect_f.width, rect_f.height);

        cv::Scalar sum = cv::sum(frame);
        int num_pixels = frame.rows * frame.cols;
        avg_chans_ = cv::Scalar(sum[0] / num_pixels, sum[1] / num_pixels, sum[2] / num_pixels);

        auto [restricted_pos, restricted_sz] = restrictBox(target_pos_, target_sz_,
                                                           im_w_, im_h_, min_w_, min_h_);

        target_pos_ = restricted_pos;
        target_sz_ = restricted_sz;

        auto [z_crop, template_scale] = getCrop(frame, target_pos_, target_sz_, z_size_, 303, 0, avg_chans_);
        static int crop_counter = 0;
        std::string filename = "/home/pi5/shared_folder/aiPlatform/images/crop_" + std::to_string(crop_counter++) + ".jpg";
        cv::imwrite(filename, z_crop);

        if (!extractFeatures(z_crop)) {
            return false;
        }

        initCosineWindow();
        is_initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

cv::Mat SiamFCPPTracker2::xyxy2cxywh(const cv::Mat& bbox) {
    try {
        if (bbox.dims != 2 || bbox.size[1] != 4) {
            return cv::Mat();
        }
        
        int rows = bbox.size[0];
        cv::Mat cxywh(rows, 4, CV_32F);
        
        for (int i = 0; i < rows; i++) {
            float x1 = bbox.at<float>(i, 0);
            float y1 = bbox.at<float>(i, 1);
            float x2 = bbox.at<float>(i, 2);
            float y2 = bbox.at<float>(i, 3);
            
            float cx = (x1 + x2) / 2.0f;
            float cy = (y1 + y2) / 2.0f;
            float w = (x2 - x1) + 1.0f;
            float h = (y2 - y1) + 1.0f;
            
            cxywh.at<float>(i, 0) = cx;
            cxywh.at<float>(i, 1) = cy;
            cxywh.at<float>(i, 2) = w;
            cxywh.at<float>(i, 3) = h;
        }
        
        return cxywh;
        
    } catch (const cv::Exception& e) {
        return cv::Mat();
    } catch (const std::exception& e) {
        return cv::Mat();
    }
}

float maxWithReciprocal(float r) {
    return std::max(r, 1.0f / r);
}

float calculateSize(float w, float h) {
    float pad = (w + h) * 0.5f;
    float sz2 = (w + pad) * (h + pad);
    return std::sqrt(sz2);
}

float calculateSize(const cv::Size2f& wh) {
    float pad = (wh.width + wh.height) * 0.5f;
    float sz2 = (wh.width + pad) * (wh.height + pad);
    return std::sqrt(sz2);
}

SiamFCPPTracker2::ScoreProcessResult SiamFCPPTracker2::postProcessScore(const std::vector<float>& score,
                                    const cv::Mat& box_wh,
                                    const cv::Size2f& target_sz,
                                    float scale_x) {
    ScoreProcessResult result;
    int num_elements = score.size();

    result.pscore.resize(num_elements);
    result.penalty.resize(num_elements);

    cv::Size2f target_sz_in_crop(target_sz.width * scale_x, target_sz.height * scale_x);
    
    float score_max = *std::max_element(score.begin(), score.end());
    float score_min = *std::min_element(score.begin(), score.end());
    float score_range = score_max - score_min;

    for (int i = 0; i < num_elements; i++) {
        float s_c = maxWithReciprocal(
                    calculateSize(box_wh.at<float>(i, 2), box_wh.at<float>(i, 3)) /
                    calculateSize(target_sz_in_crop)
                    );

        float r_c = maxWithReciprocal(
                    (target_sz_in_crop.width / target_sz_in_crop.height) /
                    (box_wh.at<float>(i, 2) / box_wh.at<float>(i, 3))
                    );

        result.penalty[i] = std::exp(-(r_c * s_c - 1.0f) * penalty_k_);
        result.pscore[i] = result.penalty[i] * score[i];

        if (i < window_.size()) {
            result.pscore[i] = result.pscore[i] * (1.0f - window_influence_) + window_[i] * window_influence_;
        }
    }

    auto max_it = std::max_element(result.pscore.begin(), result.pscore.end());
    result.best_pscore_id = std::distance(result.pscore.begin(), max_it);

    return result;
}

cv::Rect SiamFCPPTracker2::update(const cv::Mat& frame, float& confidence) {
    if (!is_initialized_) {
        confidence = 0.0f;
        return cv::Rect();
    }
    try {
        im_w_ = frame.cols;
        im_h_ = frame.rows;

        cv::Point2f prior_pos = target_pos_;
        cv::Size2f prior_sz = target_sz_;

        auto [x_crop, scale_x] = getCrop(frame, prior_pos, prior_sz, 127, x_size_, 1, avg_chans_);
        cv::Mat blob = cv::dnn::blobFromImage(
                    x_crop,
                    1.0,
                    cv::Size(x_size_, x_size_),
                    cv::Scalar(0, 0, 0),
                    true,
                    false
                    );

        std::vector<cv::String> input_names = {"im_x","c_z_k", "r_z_k"};
        std::vector<cv::String> output_names = {"bbox", "score"};
        std::vector<cv::Mat> inputs = {blob, template_features_[0], template_features_[1]};
        track_net_.setInputsNames(input_names);

        for (size_t i = 0; i < inputs.size(); i++) {
            track_net_.setInput(inputs[i], input_names[i]);
        }

        std::vector<cv::Mat> outputs;
        track_net_.forward(outputs, output_names);

        cv::Mat bbox_map = outputs[0];
        bbox_map = bbox_map.reshape(0, bbox_map.size[1]);     
        cv::Mat bbox_wh = xyxy2cxywh(bbox_map);
        cv::Mat score_map = outputs[1];
        score_map = score_map.reshape(0, score_map.size[1]);
        
        std::vector<float> score_vec;
        score_vec.assign((float*)score_map.data, (float*)score_map.data + score_map.total());
        
        ScoreProcessResult result = postProcessScore(score_vec, bbox_wh, prior_sz, scale_x);

        auto [new_pos, new_sz] = postProcessBox(
            result.best_pscore_id,
            score_vec, 
            bbox_wh, 
            prior_pos, 
            prior_sz, 
            scale_x, 
            x_size_, 
            result.penalty
        );

        auto [restricted_pos, restricted_sz] = restrictBox(
            new_pos, 
            new_sz, 
            im_w_, 
            im_h_, 
            min_w_, 
            min_h_
        );
        
        target_pos_ = restricted_pos;
        target_sz_ = restricted_sz;
        confidence = score_vec[result.best_pscore_id];
        
        const float CONFIDENCE_THRESHOLD = 0.15f;
        if (confidence < CONFIDENCE_THRESHOLD) {
            confidence = 0.0f;
        }

        cv::Rect2f rect_f = cxywhToXywh(cv::Rect2f(target_pos_.x, target_pos_.y, target_sz_.width, target_sz_.height));
        cv::Rect rect_result(
            static_cast<int>(rect_f.x),
            static_cast<int>(rect_f.y),
            static_cast<int>(rect_f.width),
            static_cast<int>(rect_f.height)
        );
        return rect_result;
    }
    catch (const std::exception& e) {
        confidence = 0.0f;
        return cv::Rect(0, 0, 0, 0);
    }
}

bool SiamFCPPTracker2::extractFeatures(const cv::Mat& z_crop) {
    try {
        cv::Mat blob = cv::dnn::blobFromImage(
                    z_crop,
                    1.0,
                    cv::Size(z_size_, z_size_),
                    cv::Scalar(0, 0, 0),
                    true,
                    false
                    );

        feature_net_.setInput(blob, "im_z");

        std::vector<cv::String> outNames = feature_net_.getUnconnectedOutLayersNames();

        std::vector<cv::Mat> outputs;
        feature_net_.forward(outputs, outNames);

        template_features_.resize(2);

        if (outputs.size() < 2) {
            return false;
        }

        template_features_[0] = outputs[0].clone();
        template_features_[1] = outputs[1].clone();

        return true;
    } catch (const cv::Exception& e) {
        return false;
    } catch (const std::exception& e) {
        return false;
    }
}

cv::Vec4f cxywh2xyxy_vec(const cv::Point2f& center, const cv::Size2f& size) {
    float x1 = center.x - (size.width - 1) / 2.0f;
    float y1 = center.y - (size.height - 1) / 2.0f;
    float x2 = center.x + (size.width - 1) / 2.0f;
    float y2 = center.y + (size.height - 1) / 2.0f;
    return cv::Vec4f(x1, y1, x2, y2);
}

cv::Mat getSubwindowTracking(const cv::Mat& im,
                             const cv::Point2f& pos,
                             int model_sz,
                             int original_sz,
                             const cv::Scalar& avg_chans) {
    float half_sz = original_sz / 2.0f;
    float x1 = pos.x - half_sz;
    float y1 = pos.y - half_sz;
    float x2 = pos.x + half_sz;
    float y2 = pos.y + half_sz;

    float scale_x = (x2 - x1) / (model_sz - 1.0f);
    float scale_y = (y2 - y1) / (model_sz - 1.0f);
    float offset_x = x1;
    float offset_y = y1;

    cv::Mat mat2x3 = (cv::Mat_<double>(2, 3) <<
                      scale_x, 0.0,    offset_x,
                      0.0,     scale_y, offset_y);

    cv::Mat im_patch;
    cv::warpAffine(im, im_patch, mat2x3, cv::Size(model_sz, model_sz),
                   cv::INTER_LINEAR | cv::WARP_INVERSE_MAP,
                   cv::BORDER_CONSTANT,
                   avg_chans);

    return im_patch;
}

cv::Rect2f cxywh2xyxy(const cv::Rect2f& box) {
    float x1 = box.x - (box.width - 1) / 2.0f;
    float y1 = box.y - (box.height - 1) / 2.0f;
    float x2 = box.x + (box.width - 1) / 2.0f;
    float y2 = box.y + (box.height - 1) / 2.0f;
    return cv::Rect2f(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}

std::pair<cv::Mat, float> SiamFCPPTracker2::getCrop(const cv::Mat& img,
                                                    const cv::Point2f& target_pos,
                                                    const cv::Size2f& target_sz,
                                                    int z_size,
                                                    int x_size,
                                                    int mode,
                                                    const cv::Scalar& avg_chans) {
    float wc = target_sz.width + context_amount_ * (target_sz.width + target_sz.height);
    float hc = target_sz.height + context_amount_ * (target_sz.width + target_sz.height);
    float s_crop = std::sqrt(wc * hc);

    float scale = static_cast<float>(z_size) / s_crop;

    int output_sz = (mode == 0) ? z_size : x_size;
    s_crop = static_cast<float>(output_sz) / scale;

    cv::Mat im_crop = getSubwindowTracking(img, target_pos, output_sz, std::round(s_crop), avg_chans);

    return {im_crop, scale};
}

void SiamFCPPTracker2::initCosineWindow() {
    if (score_size_ <= 0) {
        score_size_ = 17;
    }
    
    int total_elements = score_size_ * score_size_;
    window_.resize(total_elements);

    std::vector<float> hann_1d(score_size_);
    for (int i = 0; i < score_size_; i++) {
        hann_1d[i] = 0.5f * (1.0f - std::cos(2.0f * CV_PI * i / (score_size_ - 1)));
    }

    for (int i = 0; i < score_size_; i++) {
        for (int j = 0; j < score_size_; j++) {
            window_[i * score_size_ + j] = hann_1d[i] * hann_1d[j];
        }
    }
}

cv::Rect2f SiamFCPPTracker2::xywhToCxywh(const cv::Rect2f& rect) {
    return cv::Rect2f(
                rect.x + rect.width / 2,
                rect.y - rect.height / 2,
                rect.width,
                rect.height
                );
}

cv::Rect2f SiamFCPPTracker2::cxywhToXywh(const cv::Rect2f& rect) {
    return cv::Rect2f(
                rect.x - rect.width / 2,
                rect.y - rect.height / 2,
                rect.width,
                rect.height
                );
}

std::pair<cv::Point2f, cv::Size2f> SiamFCPPTracker2::restrictBox(const cv::Point2f& pos,
                                                                 const cv::Size2f& sz,
                                                                 int im_w, int im_h,
                                                                 float min_w, float min_h) {
    cv::Point2f new_pos = pos;
    cv::Size2f new_sz = sz;

    new_sz.width = std::max(min_w, std::min(static_cast<float>(im_w * 0.9f), sz.width));
    new_sz.height = std::max(min_h, std::min(static_cast<float>(im_h * 0.9f), sz.height));

    float half_w = new_sz.width / 2.0f;
    float half_h = new_sz.height / 2.0f;

    new_pos.x = std::max(half_w + 1.0f, std::min(static_cast<float>(im_w) - half_w - 1.0f, pos.x));
    new_pos.y = std::max(half_h + 1.0f, std::min(static_cast<float>(im_h) - half_h - 1.0f, pos.y));

    return {new_pos, new_sz};
}

std::pair<cv::Point2f, cv::Size2f> SiamFCPPTracker2::postProcessBox(int best_pscore_id,
                                                  const std::vector<float>& score,
                                                  const cv::Mat& box_wh,
                                                  const cv::Point2f& target_pos,
                                                  const cv::Size2f& target_sz,
                                                  float scale_x,
                                                  int x_size,
                                                  const std::vector<float>& penalty) {
    float pred_x = box_wh.at<float>(best_pscore_id, 0) / scale_x;
    float pred_y = box_wh.at<float>(best_pscore_id, 1) / scale_x;
    float pred_w = box_wh.at<float>(best_pscore_id, 2) / scale_x;
    float pred_h = box_wh.at<float>(best_pscore_id, 3) / scale_x;
    
    float lr = penalty[best_pscore_id] * score[best_pscore_id] * test_lr_;
    
    float res_x = pred_x + target_pos.x - (x_size / 2.0f) / scale_x;
    float res_y = pred_y + target_pos.y - (x_size / 2.0f) / scale_x;
    
    float res_w = target_sz.width * (1.0f - lr) + pred_w * lr;
    float res_h = target_sz.height * (1.0f - lr) + pred_h * lr;
    
    cv::Point2f new_target_pos(res_x, res_y);
    cv::Size2f new_target_sz(res_w, res_h);
    
    return {new_target_pos, new_target_sz};
}



