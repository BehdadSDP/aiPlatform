#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <tuple>
#include <any>

/**
 * SiamFC++ tracker implementation
 * Visual object tracking using Siamese networks with an additional transformation network
 */
class SiamFCPPTracker2 {
public:
    // Constructor takes the ONNX model path
    explicit SiamFCPPTracker2(const std::string& onnxPath, const std::string& tonnxPath);

    // Initialize with the first frame and bounding box
    void init(const cv::Mat& frame, const std::vector<double>& bbox);

    // Update tracking on a subsequent frame, returns the new bounding box
    cv::Rect update(const cv::Mat& frame);

    // Check if tracker is initialized
    bool isInitialized() const;

    // Get tracking confidence score
    float getTrackingScore() const;

private:
    // Neural network for SiamFC++ model
    cv::dnn::Net net_;
    cv::dnn::Net tnet_;

    std::vector<float> createHanning(int size);
    std::tuple<double, double, double, double> get_axis_aligned_bbox(const std::vector<double>& region);
    std::vector<float> xywh2cxywh(const std::vector<float>& rect);
    std::tuple<cv::Mat, float> get_crop(
            const cv::Mat& im,
            const std::vector<float>& target_pos,
            const std::vector<float>& target_sz,
            int z_size,
            int x_size = -1,
            const std::vector<float>& avg_chans = {0, 0, 0},
            float context_amount = 0.5,
            std::function<cv::Mat(const cv::Mat&, const std::vector<float>&, int, int, const std::vector<float>&)> func_get_subwindow = nullptr,
            int output_size = -1,
            const cv::Mat& mask = cv::Mat());
    cv::Mat get_subwindow_tracking(
            const cv::Mat& im,
            const std::vector<float>& pos,
            int model_sz,
            int original_sz,
            const std::vector<float>& avg_chans = {0, 0, 0},
            const cv::Mat& mask = cv::Mat());
    std::vector<float> cxywh2xyxy(const std::vector<float>& box);
    cv::Mat xyxy2cxywh(const cv::Mat& bbox);
    std::tuple<int, std::vector<float>, std::vector<float>>
    postprocessScore(const std::vector<float>& score,
                     const std::vector<float>& box_wh,
                     const cv::Size2f& target_sz,
                     float scale_x,
                     const std::vector<float>& window,
                     float penalty_k,
                     float window_influence);
    std::pair<cv::Point2f, cv::Size2f>
    postprocessBox(int best_pscore_id,
                   const std::vector<float>& score,
                   const std::vector<float>& box_wh,
                   const cv::Point2f& target_pos,
                   const cv::Size2f& target_sz,
                   float scale_x,
                   int x_size,
                   const std::vector<float>& penalty,
                   float test_lr);

    std::pair<cv::Point2f, cv::Size2f>
    restrictBox(const cv::Point2f& target_pos,
                const cv::Size2f& target_sz,
                int im_w,
                int im_h,
                float min_w,
                float min_h);

    // Internal state storage
    std::map<std::string, std::any> _state;

    // Hyperparameters
    std::map<std::string, std::any> _hyper_params;
};
