#pragma once
#ifndef SIAMFCPP_TRACKER2_H
#define SIAMFCPP_TRACKER2_H

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>
#include <vector>
#include <string>
#include "../tracker_interface.h"

class SiamFCPPTracker2 : public TrackerInterface {
public:
    // Constructor/Destructor
    SiamFCPPTracker2();
    ~SiamFCPPTracker2();

    // TrackerInterface implementation
    bool init(const cv::Mat& frame, const cv::Rect& initBox) override;
    cv::Rect update(const cv::Mat& frame) override;
    bool isInitialized() const override { return is_initialized_; }
    float getLastConfidence() const override { return lastConfidence_; }

    // Model loading (called before init)
    bool loadModel(const std::string& feature_model_path, const std::string& track_model_path);
    
    // Crop function - made public to allow external access if needed
    std::pair<cv::Mat, float> getCrop(const cv::Mat& img,
                                      const cv::Point2f& target_pos,
                                      const cv::Size2f& target_sz,
                                      int z_size,
                                      int x_size,
                                      int mode,
                                      const cv::Scalar& avg_chans);
private:
    // Model parameters
    int z_size_;           // Template image size
    int x_size_;           // Search region size
    float context_amount_; // Context amount for cropping
    int score_size_;       // Output score map size
    int stride_;           // Total stride of backbone
    float penalty_k_;      // Penalty for scale change
    float window_influence_; // Window influence factor
    float test_lr_;        // Learning rate for target size update
    float min_w_;          // Minimum allowed width
    float min_h_;          // Minimum allowed height

    // State variables
    cv::Point2f target_pos_;  // Target position (center x, center y)
    cv::Size2f target_sz_;    // Target size (width, height)
    cv::Scalar avg_chans_;    // Average channel values
    bool is_initialized_;     // Initialization flag
    float scale_z_;           // Scale used for the template patch
    int im_w_;                // Image width
    int im_h_;                // Image height
    float lastConfidence_;    // Last confidence score

    // Cosine window for penalizing
    std::vector<float> window_;

    // OpenCV DNN models
    cv::dnn::Net feature_net_;
    cv::dnn::Net track_net_;

    // Template features from initialization
    std::vector<cv::Mat> template_features_;

    // Helper methods
    bool extractFeatures(const cv::Mat& z_crop);

    void initCosineWindow();
    cv::Mat xyxy2cxywh(const cv::Mat& bbox);
    cv::Rect2f xywhToCxywh(const cv::Rect2f& rect);
    cv::Rect2f cxywhToXywh(const cv::Rect2f& rect);
    std::pair<cv::Point2f, cv::Size2f> restrictBox(const cv::Point2f& pos,
                                                   const cv::Size2f& sz,
                                                   int im_w, int im_h,
                                                   float min_w, float min_h);

    // In the private section of the SiamFCPPTracker2 class
    struct ScoreProcessResult {
        int best_pscore_id;
        std::vector<float> pscore;
        std::vector<float> penalty;
    };

    ScoreProcessResult postProcessScore(const std::vector<float>& score,
                                    const cv::Mat& box_wh,
                                    const cv::Size2f& target_sz,
                                    float scale_x
                                    );

    std::pair<cv::Point2f, cv::Size2f> postProcessBox(int best_pscore_id,
                                                     const std::vector<float>& score,
                                                     const cv::Mat& box_wh,
                                                     const cv::Point2f& target_pos,
                                                     const cv::Size2f& target_sz,
                                                     float scale_x,
                                                     int x_size,
                                                     const std::vector<float>& penalty);
};

#endif // SIAMFCPP_TRACKER2_H
