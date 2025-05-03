#include "siamfc_pp_tracker.h"
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <iomanip> // For formatting output
#include <fstream>

// Add this helper function before the SiamFCPPTracker2 class definitions
struct MinMaxValues {
    float min_val;
    float max_val;
};

// Helper function to safely calculate min/max values for tensors of any dimension
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

// Helper function to print tensor dimensions
void printTensorShape(const cv::Mat& tensor, const std::string& name) {
    std::cout << name << " dimensions: " << tensor.dims << ", shape: ";
    for (int i = 0; i < tensor.dims; i++) {
        std::cout << tensor.size[i] << " ";
    }
    std::cout << std::endl;
}

// Helper function to calculate scale penalty
float calculateScalePenalty(const cv::Size2f& target_sz, const cv::Size2f& candidate_sz) {
    // Calculate change in size ratio
    float w_ratio = candidate_sz.width / target_sz.width;
    float h_ratio = candidate_sz.height / target_sz.height;

    // Calculate scale change
    float scale_change = (w_ratio * h_ratio) - 1.0f;

    // Apply penalty formula (constant from the class)
    float penalty_k = 0.04f; // Same as the class's penalty_k_ value
    return std::exp(-scale_change * scale_change / penalty_k);
}

SiamFCPPTracker2::SiamFCPPTracker2() {
    // Initialize default parameters
    z_size_ = 127;      // Template size
    x_size_ = 303;      // Search region size
    context_amount_ = 0.5f;  // Context region multiplier - INCREASED from 0.1 for more context

    // Tracking parameters - using values from SiamFC++ paper
    score_size_ = 17;   // Score map size
    stride_ = 8;        // Total stride of network
    penalty_k_ = 0.04f; // Penalty for scale change
    window_influence_ = 0.21f; // Cosine window influence factor
    test_lr_ = 0.52f;   // Learning rate for target size update
    min_w_ = 10.0f;     // Minimum target width
    min_h_ = 10.0f;     // Minimum target height

    std::cout << "[SiamFCPPTracker2] Initializing tracker with parameters:" << std::endl;
    std::cout << "  z_size: " << z_size_ << std::endl;
    std::cout << "  x_size: " << x_size_ << std::endl;
    std::cout << "  context_amount: " << context_amount_ << std::endl;
    std::cout << "  score_size: " << score_size_ << std::endl;
    std::cout << "  stride: " << stride_ << std::endl;
    std::cout << "  penalty_k: " << penalty_k_ << std::endl;
    std::cout << "  window_influence: " << window_influence_ << std::endl;
    std::cout << "  test_lr: " << test_lr_ << std::endl;
    std::cout << "  min_w/min_h: " << min_w_ << "/" << min_h_ << std::endl;

    // Initialize cosine window
    initCosineWindow();
    
    // Initialize template_features_ vector
    template_features_.resize(2);

    // Set initialization flag
    is_initialized_ = false;
}

SiamFCPPTracker2::~SiamFCPPTracker2() = default;

bool SiamFCPPTracker2::loadModel(const std::string& feature_model_path, const std::string& track_model_path) {
    try {
        // Load feature extraction model with OpenCV DNN
        feature_net_ = cv::dnn::readNetFromONNX(feature_model_path);
        if (feature_net_.empty()) {
            std::cerr << "Failed to load feature model: " << feature_model_path << std::endl;
            return false;
        }

        // Load tracking model with OpenCV DNN
        track_net_ = cv::dnn::readNetFromONNX(track_model_path);
        if (track_net_.empty()) {
            std::cerr << "Failed to load track model: " << track_model_path << std::endl;
            return false;
        }

        // Optimize networks for inference
        feature_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        feature_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        track_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        track_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        std::cout << "Models loaded successfully" << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV Error: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error loading model: " << e.what() << std::endl;
        return false;
    }
}

bool SiamFCPPTracker2::init(const cv::Mat& frame, const cv::Rect& bbox) {
    try {

        //debug
        std::cout << "[siam-init]: bounding box from yolo: ["
                  << bbox.x << ',' << bbox.y << ',' << bbox.width << ',' << bbox.height << ']' << std::endl;

        cv::Rect adjBbox = bbox;
        adjBbox.y = int(bbox.y + bbox.height );
        //adjusted box
        adjBbox.x = adjBbox.x + (adjBbox.width/2);
        adjBbox.y = adjBbox.y - (adjBbox.height/2);

        //debug
        std::cout << "[siam-init]: adj bounding box: ["
                  << adjBbox.x << ',' << adjBbox.y << ',' << adjBbox.width << ',' << adjBbox.height << ']' << std::endl;

        adjBbox.x = adjBbox.x - ((adjBbox.width - 1)/2);
        adjBbox.y = adjBbox.y + ((adjBbox.height - 1)/2);

        //debug
        std::cout << "[siam-init]: gt bounding box: ["
                  << adjBbox.x << ',' << adjBbox.y << ',' << adjBbox.width << ',' << adjBbox.height << ']' << std::endl;

        adjBbox = xywhToCxywh(adjBbox);

        //debug
        std::cout << "[siam-init]: xy2cxy bounding box: ["
                  << adjBbox.x << ',' << adjBbox.y << ',' << adjBbox.width << ',' << adjBbox.height << ']' << std::endl;

        // Save image size for boundary checking
        im_w_ = frame.cols;
        im_h_ = frame.rows;

        //debug
        std::cout << "[siam-init]: Image dimensions: " << im_w_ << "x" << im_h_ << std::endl;

        // Convert bbox to floating point
        cv::Rect2f rect_f(static_cast<float>(adjBbox.x),
                          static_cast<float>(adjBbox.y),
                          static_cast<float>(adjBbox.width),
                          static_cast<float>(adjBbox.height));

        target_pos_ = cv::Point2f(rect_f.x, rect_f.y);
        target_sz_ = cv::Size2f(rect_f.width, rect_f.height);

        //debug
        std::cout << "[siam-init]: Target position: " << target_pos_.x << ", " << target_pos_.y << std::endl;
        std::cout << "[siam-init]: Target size: " << target_sz_.width << ", " << target_sz_.height << std::endl;

        // Calculate average color (used for padding)
        cv::Scalar sum = cv::sum(frame);
        int num_pixels = frame.rows * frame.cols;
        avg_chans_ = cv::Scalar(sum[0] / num_pixels, sum[1] / num_pixels, sum[2] / num_pixels);
        std::cout << "[siam-init]: Average channels: " << avg_chans_[0] << ", " << avg_chans_[1] << ", " << avg_chans_[2] << std::endl;

        // Handle boundary conditions
        auto [restricted_pos, restricted_sz] = restrictBox(target_pos_, target_sz_,
                                                           im_w_, im_h_, min_w_, min_h_);

        // Debug output
        std::cout << "[siam-init]: restrictBox: orig_pos=(" << target_pos_.x << "," << target_pos_.y
                  << "), orig_sz=(" << target_sz_.width << "," << target_sz_.height
                  << "), new_pos=(" << restricted_pos.x << "," << restricted_pos.y
                  << "), new_sz=(" << restricted_sz.width << "," << restricted_sz.height << ")" << std::endl;

        target_pos_ = restricted_pos;
        target_sz_ = restricted_sz;

        // Extract template patch
        auto [z_crop, template_scale] = getCrop(frame, target_pos_, target_sz_, z_size_, 303, 0, avg_chans_);
        //save the image
        static int crop_counter = 0;
        std::string filename = "/home/pi5/shared_folder/aiPlatform/images/crop_" + std::to_string(crop_counter++) + ".jpg";
        cv::imwrite(filename, z_crop);

        // Extract template features using the feature model
        if (!extractFeatures(z_crop)) {
            std::cerr << "Failed to extract features" << std::endl;
            return false;
        }

        initCosineWindow();
        // Set initialization flag
        is_initialized_ = true;
        std::cout << "Tracker initialized successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing tracker: " << e.what() << std::endl;
        return false;
    }
}

cv::Mat SiamFCPPTracker2::xyxy2cxywh(const cv::Mat& bbox) {
    // Make sure bbox has 4 elements
    try {

        // Check if we have the right data format
        if (bbox.dims != 2 || bbox.size[1] != 4) {
            std::cerr << "Error: bbox should be a Nx4 matrix, got " << bbox.dims
                      << " dimensions with size " << (bbox.dims > 0 ? bbox.size[0] : 0)
                      << "x" << (bbox.dims > 1 ? bbox.size[1] : 0) << std::endl;
            // Return empty matrix on error
            return cv::Mat();
        }
        
        // Create output matrix with same number of rows as input
        int rows = bbox.size[0];
        cv::Mat cxywh(rows, 4, CV_32F);
        
        // Process each row
        for (int i = 0; i < rows; i++) {
            float x1 = bbox.at<float>(i, 0);
            float y1 = bbox.at<float>(i, 1);
            float x2 = bbox.at<float>(i, 2);
            float y2 = bbox.at<float>(i, 3);
            
            // Compute center x, center y, width, height
            float cx = (x1 + x2) / 2.0f;
            float cy = (y1 + y2) / 2.0f;
            float w = (x2 - x1) + 1.0f;
            float h = (y2 - y1) + 1.0f;
            
            // Store in output matrix
            cxywh.at<float>(i, 0) = cx;
            cxywh.at<float>(i, 1) = cy;
            cxywh.at<float>(i, 2) = w;
            cxywh.at<float>(i, 3) = h;
        }
        
        // Remove debug output
        // std::cout << "xyxy2cxywh: Output cxywh dimensions: " << cxywh.size << std::endl;
        return cxywh;
        
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV Error in xyxy2cxywh: " << e.what() << std::endl;
        return cv::Mat();
    } catch (const std::exception& e) {
        std::cerr << "Error in xyxy2cxywh: " << e.what() << std::endl;
        return cv::Mat();
    }
}


// Function to calculate maximum of a value and its reciprocal
float maxWithReciprocal(float r) {
    return std::max(r, 1.0f / r);
}

// Calculate size for penalty calculation
float calculateSize(float w, float h) {
    float pad = (w + h) * 0.5f;
    float sz2 = (w + pad) * (h + pad);
    return std::sqrt(sz2);
}

// Calculate size from Size2f for penalty calculation
float calculateSize(const cv::Size2f& wh) {
    float pad = (wh.width + wh.height) * 0.5f;
    float sz2 = (wh.width + pad) * (wh.height + pad);
    return std::sqrt(sz2);
}

SiamFCPPTracker2::ScoreProcessResult SiamFCPPTracker2::postProcessScore(const std::vector<float>& score,
                                    const cv::Mat& box_wh,
                                    const cv::Size2f& target_sz,
                                    float scale_x
                                    ) {
    /*
     * Perform SiameseRPN-based tracker's post-processing of score
     *
     * Parameters:
     *   score: (HW, ), score prediction
     *   box_wh: (HW, 4), cxywh, bbox prediction
     *   target_sz: previous state (w & h)
     *   scale_x: scale factor
     *   penalty_k: penalty for scale/ratio change
     *   window_influence: cosine window influence factor
     *   window: cosine window values
     *
     * Returns:
     *   ScoreProcessResult containing:
     *     best_pscore_id: index of chosen candidate along axis HW
     *     pscore: (HW, ), penalized score
     *     penalty: (HW, ), penalty due to scale/ratio change
     */

    ScoreProcessResult result;
    int num_elements = score.size();

    // Initialize result vectors
    result.pscore.resize(num_elements);
    result.penalty.resize(num_elements);

    // Size penalty
    cv::Size2f target_sz_in_crop(target_sz.width * scale_x, target_sz.height * scale_x);
    
    // Normalize scores for better comparison
    float score_max = *std::max_element(score.begin(), score.end());
    float score_min = *std::min_element(score.begin(), score.end());
    float score_range = score_max - score_min;
    
    std::cout << "[postProcessScore] Raw score range: min=" << score_min 
              << ", max=" << score_max << ", range=" << score_range << std::endl;

    // Calculate penalties for each element
    for (int i = 0; i < num_elements; i++) {
        // Scale penalty
        float s_c = maxWithReciprocal(
                    calculateSize(box_wh.at<float>(i, 2), box_wh.at<float>(i, 3)) /
                    calculateSize(target_sz_in_crop)
                    );

        // Ratio penalty
        float r_c = maxWithReciprocal(
                    (target_sz_in_crop.width / target_sz_in_crop.height) /
                    (box_wh.at<float>(i, 2) / box_wh.at<float>(i, 3))
                    );

        // Combined penalty
        result.penalty[i] = std::exp(-(r_c * s_c - 1.0f) * penalty_k_);

        // Apply penalty to score
        result.pscore[i] = result.penalty[i] * score[i];

        // Apply cosine window (motion model)
        // Make sure window_ and pscore vectors are the same size
        if (i < window_.size()) {
            result.pscore[i] = result.pscore[i] * (1.0f - window_influence_) + window_[i] * window_influence_;
        }
    }

    // Find best score
    auto max_it = std::max_element(result.pscore.begin(), result.pscore.end());
    result.best_pscore_id = std::distance(result.pscore.begin(), max_it);

    // Debug output
    std::cout << "[postProcessScore] Window influence: " << window_influence_ << std::endl;
    std::cout << "[postProcessScore] Penalty_k: " << penalty_k_ << std::endl;
    std::cout << "[postProcessScore] Original score: " << score[result.best_pscore_id] 
              << ", Penalty: " << result.penalty[result.best_pscore_id]
              << ", Penalized score: " << result.pscore[result.best_pscore_id] << std::endl;

    return result;
}


cv::Rect SiamFCPPTracker2::update(const cv::Mat& frame, float& confidence) {
    if (!is_initialized_) {
        std::cerr << "[siam-update] ERROR: Tracker not initialized" << std::endl;
        confidence = 0.0f;
        return cv::Rect();
    }

    try {
        std::cout << "\n[siam-update] ===== Starting tracking update =====" << std::endl;
        
        // Update image size for boundary checking
        im_w_ = frame.cols;
        im_h_ = frame.rows;
        std::cout << "[siam-update] Frame size: " << im_w_ << "x" << im_h_ << std::endl;

        cv::Point2f prior_pos = target_pos_;
        cv::Size2f prior_sz = target_sz_;
        std::cout << "[siam-update] Prior target position: " << prior_pos.x << ", " << prior_pos.y << std::endl;
        std::cout << "[siam-update] Prior target size: " << prior_sz.width << ", " << prior_sz.height << std::endl;

        // Get search region crop (passing x_size_ now)
        auto [x_crop, scale_x] = getCrop(frame, prior_pos, prior_sz, 127, x_size_, 1, avg_chans_);
        std::cout << "[siam-update] x_crop size: " << x_crop.cols << "x" << x_crop.rows << ", scale_x: " << scale_x << std::endl;

        //save the image
        static int crop_counter = 0;
        std::string filename = "/home/pi5/shared_folder/aiPlatform/images/xcrop_" + std::to_string(crop_counter++) + ".jpg";
        cv::imwrite(filename, x_crop);

        // Prepare input blob
        cv::Mat blob = cv::dnn::blobFromImage(
                    x_crop,
                    1.0,               // Scale factor
                    cv::Size(x_size_, x_size_), // Target size
                    cv::Scalar(0, 0, 0),       // Mean
                    true,                      // swapRB
                    false                      // Crop
                    );
        std::cout << "[siam-update] Created input blob with size: " << blob.size[0] << "x" << blob.size[1] << "x" << blob.size[2] << "x" << blob.size[3] << std::endl;

        // Define input and output names
        std::vector<cv::String> input_names = {"im_x","c_z_k", "r_z_k"};
        std::vector<cv::String> output_names = {"bbox", "score"};

        // Check if template features have valid data
        if (template_features_.size() < 2 || template_features_[0].empty() || template_features_[1].empty()) {
            std::cerr << "[siam-update] ERROR: Template features are not properly initialized" << std::endl;
            confidence = 0.0f;
            return cv::Rect(0, 0, 0, 0);
        }
        
        // Calculate min/max values of template features
        MinMaxValues minmax0 = calculateMinMax(template_features_[0]);
        MinMaxValues minmax1 = calculateMinMax(template_features_[1]);
        
        std::cout << "[siam-update] Template feature [0] min: " << minmax0.min_val << ", max: " << minmax0.max_val << std::endl;
        std::cout << "[siam-update] Template feature [1] min: " << minmax1.min_val << ", max: " << minmax1.max_val << std::endl;

        // Set inputs to the network
        std::vector<cv::Mat> inputs = {blob, template_features_[0], template_features_[1]};
        track_net_.setInputsNames(input_names);

        for (size_t i = 0; i < inputs.size(); i++) {
            track_net_.setInput(inputs[i], input_names[i]);
        }
        std::cout << "[siam-update] Set inputs to tracking network" << std::endl;

        // Run forward pass
        std::vector<cv::Mat> outputs;
        track_net_.forward(outputs, output_names);
        std::cout << "[siam-update] Completed forward pass, got " << outputs.size() << " outputs" << std::endl;
        
        cv::Mat bbox_map = outputs[0];
        // Reshape bbox_map from [1, 289, 4] to [289, 4]
        bbox_map = bbox_map.reshape(0, bbox_map.size[1]);

        // Debug code for checking bbox_map dimensions
        std::cout << "[siam-update] bbox_map dimensions: " << bbox_map.dims << std::endl;
        std::cout << "[siam-update] bbox_map size: ";
        for (int i = 0; i < bbox_map.dims; ++i) {
            std::cout << bbox_map.size[i] << " ";
        }
        std::cout << std::endl;
        
        // Debug some bbox values
        std::cout << "[siam-update] First few bboxes from network:" << std::endl;
        for (int i = 0; i < std::min(5, bbox_map.size[0]); i++) {
            std::cout << "  Box " << i << ": [" 
                      << bbox_map.at<float>(i, 0) << ", " 
                      << bbox_map.at<float>(i, 1) << ", " 
                      << bbox_map.at<float>(i, 2) << ", " 
                      << bbox_map.at<float>(i, 3) << "]" << std::endl;
        }
        
        cv::Mat bbox_wh = xyxy2cxywh(bbox_map);
        // Debug code to check bbox_wh dimensions
        std::cout << "[siam-update] bbox_wh dimensions: " << bbox_wh.dims << std::endl;
        std::cout << "[siam-update] bbox_wh size: ";
        for (int i = 0; i < bbox_wh.dims; ++i) {
            std::cout << bbox_wh.size[i] << " ";
        }
        std::cout << std::endl;
        
        // Debug some converted boxes
        std::cout << "[siam-update] First few cxywh boxes:" << std::endl;
        for (int i = 0; i < std::min(5, bbox_wh.size[0]); i++) {
            std::cout << "  Box " << i << ": [" 
                      << bbox_wh.at<float>(i, 0) << ", " 
                      << bbox_wh.at<float>(i, 1) << ", " 
                      << bbox_wh.at<float>(i, 2) << ", " 
                      << bbox_wh.at<float>(i, 3) << "]" << std::endl;
        }

        cv::Mat score_map = outputs[1];
        score_map = score_map.reshape(0, score_map.size[1]); // (289, 1)
        
        // Convert score_map to vector for processing
        std::vector<float> score_vec;
        score_vec.assign((float*)score_map.data, (float*)score_map.data + score_map.total());
        
        // Debug score values
        std::cout << "[siam-update] Score vector size: " << score_vec.size() << std::endl;
        float min_score = *std::min_element(score_vec.begin(), score_vec.end());
        float max_score = *std::max_element(score_vec.begin(), score_vec.end());
        float avg_score = std::accumulate(score_vec.begin(), score_vec.end(), 0.0f) / score_vec.size();
        std::cout << "[siam-update] Score range - min: " << min_score << ", max: " << max_score << ", avg: " << avg_score << std::endl;
        
        // Process score
        ScoreProcessResult result = postProcessScore(score_vec, bbox_wh, prior_sz, scale_x);
        std::cout << "[siam-update] Best score index: " << result.best_pscore_id << std::endl;
        std::cout << "[siam-update] Best original score: " << score_vec[result.best_pscore_id] << std::endl;
        std::cout << "[siam-update] Best penalized score: " << result.pscore[result.best_pscore_id] << std::endl;
        std::cout << "[siam-update] Applied penalty: " << result.penalty[result.best_pscore_id] << std::endl;
        
        // Post-process box to get new target position and size
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
        
        std::cout << "[siam-update] New position after postProcessBox: (" << new_pos.x << ", " << new_pos.y << ")" << std::endl;
        std::cout << "[siam-update] New size after postProcessBox: (" << new_sz.width << ", " << new_sz.height << ")" << std::endl;
        
        // Restrict target position and size
        auto [restricted_pos, restricted_sz] = restrictBox(
            new_pos, 
            new_sz, 
            im_w_, 
            im_h_, 
            min_w_, 
            min_h_
        );
        
        std::cout << "[siam-update] Restricted position: (" << restricted_pos.x << ", " << restricted_pos.y << ")" << std::endl;
        std::cout << "[siam-update] Restricted size: (" << restricted_sz.width << ", " << restricted_sz.height << ")" << std::endl;
        
        // Update tracker state
        target_pos_ = restricted_pos;
        target_sz_ = restricted_sz;
        
        // Set confidence using the best score
        confidence = score_vec[result.best_pscore_id];
        
        // Apply confidence boosting to handle models that output low confidence values
        // This scaling preserves the relative confidence between different frames
        const float CONFIDENCE_BOOST = 15.0f;  // Boost factor
        confidence = std::min(confidence * CONFIDENCE_BOOST, 1.0f);
        
        std::cout << "[siam-update] Original confidence: " << score_vec[result.best_pscore_id] 
                  << ", Boosted confidence: " << confidence << std::endl;
        
        // Add confidence threshold check
        const float CONFIDENCE_THRESHOLD = 0.15f; // Adjust this threshold as needed
        if (confidence < CONFIDENCE_THRESHOLD) {
            std::cout << "[siam-update] WARNING: Confidence too low (" << confidence 
                      << " < " << CONFIDENCE_THRESHOLD << "), tracking may be unreliable" << std::endl;
            
            // If confidence is extremely low, you might want to return empty rect
            // Uncomment these lines to fail tracking on very low confidence
            // if (confidence < 0.01f) {
            //    return cv::Rect(0, 0, 0, 0);
            // }
        }
        
        // Convert to OpenCV rect and return
        cv::Rect2f rect_f = cxywhToXywh(cv::Rect2f(target_pos_.x, target_pos_.y, target_sz_.width, target_sz_.height));
        std::cout << "[siam-update] Final rect (float): [" << rect_f.x << ", " << rect_f.y << ", " << rect_f.width << ", " << rect_f.height << "]" << std::endl;
        
        cv::Rect rect_result(
            static_cast<int>(rect_f.x),
            static_cast<int>(rect_f.y),
            static_cast<int>(rect_f.width),
            static_cast<int>(rect_f.height)
        );
        
        std::cout << "[siam-update] Final rect (int): [" << rect_result.x << ", " << rect_result.y << ", " << rect_result.width << ", " << rect_result.height << "]" << std::endl;
        std::cout << "[siam-update] ===== Tracking update complete =====" << std::endl;
        
        return rect_result;
    }
    catch (const std::exception& e) {
        std::cerr << "[siam-update] Error during tracking: " << e.what() << std::endl;
        confidence = 0.0f;
        return cv::Rect(0, 0, 0, 0);
    }
}

bool SiamFCPPTracker2::extractFeatures(const cv::Mat& z_crop) {
    try {
        // Prepare input blob - OpenCV DNN expects NCHW format
        cv::Mat blob = cv::dnn::blobFromImage(
                    z_crop,
                    1.0,                           // No scaling
                    cv::Size(z_size_, z_size_),    // Target size
                    cv::Scalar(0, 0, 0),           // No mean subtraction (no effect since scale=1.0)
                    true,                          // swapRB
                    false                          // Do not crop
                    );

        // Set the input to the feature extraction network
        feature_net_.setInput(blob, "im_z");

        // Get output layer names
        std::vector<cv::String> outNames = feature_net_.getUnconnectedOutLayersNames();

        // Forward pass
        std::vector<cv::Mat> outputs;
        feature_net_.forward(outputs, outNames);

        // Keep minimal output count for important information
        std::cout << "Feature network outputs: " << outputs.size() << std::endl;

        // Make sure template_features_ has enough space
        template_features_.resize(2);

        // Check if we have enough outputs and they have the expected format
        if (outputs.size() < 2) {
            std::cerr << "Error: Not enough outputs from feature network. Expected 2, got "
                      << outputs.size() << std::endl;
            return false;
        }

        // Store the outputs
        template_features_[0] = outputs[0].clone();
        template_features_[1] = outputs[1].clone();

        // Print tensor shapes
        printTensorShape(template_features_[0], "Template feature [0]");
        printTensorShape(template_features_[1], "Template feature [1]");

        // Calculate min/max values
        MinMaxValues minmax0 = calculateMinMax(template_features_[0]);
        MinMaxValues minmax1 = calculateMinMax(template_features_[1]);

        std::cout << "Template feature [0] min: " << minmax0.min_val << ", max: " << minmax0.max_val << std::endl;
        std::cout << "Template feature [1] min: " << minmax1.min_val << ", max: " << minmax1.max_val << std::endl;

        // Keep feature shape info as it's important
        std::cout << "Features extracted successfully" << std::endl;

        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV Error extracting features: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error extracting features: " << e.what() << std::endl;
        return false;
    }
}

// Helper function to convert center-based box (cx, cy, w, h) to corner-based (x1, y1, x2, y2) vector
cv::Vec4f cxywh2xyxy_vec(const cv::Point2f& center, const cv::Size2f& size) {
    float x1 = center.x - (size.width - 1) / 2.0f;
    float y1 = center.y - (size.height - 1) / 2.0f;
    float x2 = center.x + (size.width - 1) / 2.0f;
    float y2 = center.y + (size.height - 1) / 2.0f;
    return cv::Vec4f(x1, y1, x2, y2);
}

// Implements the get_subwindow_tracking logic using warpAffine
cv::Mat getSubwindowTracking(const cv::Mat& im,
                             const cv::Point2f& pos,
                             int model_sz,           // Output size (e.g., 127 or 303)
                             int original_sz,        // Size of the crop in the original image
                             const cv::Scalar& avg_chans) {
    // Calculate the source bounding box corners in the original image
    float half_sz = original_sz / 2.0f;
    float x1 = pos.x - half_sz;
    float y1 = pos.y - half_sz;
    float x2 = pos.x + half_sz;
    float y2 = pos.y + half_sz;

    // Compute the inverse transformation matrix
    // Output coordinates (u, v) in [0, model_sz-1] map to input coordinates (x, y) in [x1, x2] x [y1, y2]
    float scale_x = (x2 - x1) / (model_sz - 1.0f); // Scale from output to input
    float scale_y = (y2 - y1) / (model_sz - 1.0f);
    float offset_x = x1; // Top-left corner of source region
    float offset_y = y1;

    // Inverse transformation matrix (maps output (u,v) to input (x,y))
    cv::Mat mat2x3 = (cv::Mat_<double>(2, 3) <<
                      scale_x, 0.0,    offset_x,
                      0.0,     scale_y, offset_y);

    // Apply warpAffine with inverse mapping
    cv::Mat im_patch;
    cv::warpAffine(im, im_patch, mat2x3, cv::Size(model_sz, model_sz),
                   cv::INTER_LINEAR | cv::WARP_INVERSE_MAP,
                   cv::BORDER_CONSTANT,
                   avg_chans);

    return im_patch;
}

// Helper function to convert center-based box (cx, cy, w, h) to corner-based (x, y, w, h)
cv::Rect2f cxywh2xyxy(const cv::Rect2f& box) {
    float x1 = box.x - (box.width - 1) / 2.0f;
    float y1 = box.y - (box.height - 1) / 2.0f;
    float x2 = box.x + (box.width - 1) / 2.0f;
    float y2 = box.y + (box.height - 1) / 2.0f;
    // Return as x, y, width, height
    return cv::Rect2f(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}

std::pair<cv::Mat, float> SiamFCPPTracker2::getCrop(const cv::Mat& img,
                                                    const cv::Point2f& target_pos,
                                                    const cv::Size2f& target_sz,
                                                    int z_size,
                                                    int x_size,
                                                    int mode,
                                                    const cv::Scalar& avg_chans) {
    // Calculate context size
    float wc = target_sz.width + context_amount_ * (target_sz.width + target_sz.height);
    float hc = target_sz.height + context_amount_ * (target_sz.width + target_sz.height);
    float s_crop = std::sqrt(wc * hc);

    // Fixed scale based on z_size
    float scale = static_cast<float>(z_size) / s_crop;

    // If x_size is provided (search mode)
    int output_sz = (mode == 0) ? z_size : x_size;
    s_crop = static_cast<float>(output_sz) / scale;  // recalculate s_crop

    // Extract crop
    cv::Mat im_crop = getSubwindowTracking(img, target_pos, output_sz, std::round(s_crop), avg_chans);

    return {im_crop, scale};
}

void SiamFCPPTracker2::initCosineWindow() {
    // Make sure score_size_ is properly set before calling this function
    std::cout << "[initCosineWindow] Creating cosine window with score_size: " << score_size_ << std::endl;
    
    // Check if score_size_ is valid
    if (score_size_ <= 0) {
        std::cerr << "[initCosineWindow] ERROR: Invalid score_size: " << score_size_ << std::endl;
        score_size_ = 17; // Default fallback
        std::cout << "[initCosineWindow] Using default score_size: " << score_size_ << std::endl;
    }
    
    // SiamFC++ typically uses 17x17 score maps
    int total_elements = score_size_ * score_size_;
    window_.resize(total_elements);

    // Create 1D cosine windows
    std::vector<float> hann_1d(score_size_);
    for (int i = 0; i < score_size_; i++) {
        hann_1d[i] = 0.5f * (1.0f - std::cos(2.0f * CV_PI * i / (score_size_ - 1)));
    }

    // Create 2D cosine window
    for (int i = 0; i < score_size_; i++) {
        for (int j = 0; j < score_size_; j++) {
            window_[i * score_size_ + j] = hann_1d[i] * hann_1d[j];
        }
    }
    
    // Debug output
    std::cout << "[initCosineWindow] Window size: " << window_.size() << std::endl;
    
    // Print a few window values to verify
    std::cout << "[initCosineWindow] Window values sample: ";
    for (int i = 0; i < std::min(5, (int)window_.size()); i++) {
        std::cout << window_[i] << " ";
    }
    std::cout << "..." << std::endl;
}

cv::Rect2f SiamFCPPTracker2::xywhToCxywh(const cv::Rect2f& rect) {
    return cv::Rect2f(
                rect.x + rect.width / 2,  // cx
                rect.y - rect.height / 2, // cy
                rect.width,               // w
                rect.height               // h
                );
}

cv::Rect2f SiamFCPPTracker2::cxywhToXywh(const cv::Rect2f& rect) {
    return cv::Rect2f(
                rect.x - rect.width / 2,  // x
                rect.y - rect.height / 2, // y
                rect.width,               // w
                rect.height               // h
                );
}

std::pair<cv::Point2f, cv::Size2f> SiamFCPPTracker2::restrictBox(const cv::Point2f& pos,
                                                                 const cv::Size2f& sz,
                                                                 int im_w, int im_h,
                                                                 float min_w, float min_h) {
    cv::Point2f new_pos = pos;
    cv::Size2f new_sz = sz;

    // Restrict size to be within valid range and image boundaries
    new_sz.width = std::max(min_w, std::min(static_cast<float>(im_w * 0.9f), sz.width));
    new_sz.height = std::max(min_h, std::min(static_cast<float>(im_h * 0.9f), sz.height));

    // Calculate half sizes for boundary checking
    float half_w = new_sz.width / 2.0f;
    float half_h = new_sz.height / 2.0f;

    // Ensure the position is far enough from image boundaries that the box doesn't go outside
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
    /*
     * Perform SiameseRPN-based tracker's post-processing of box
     *
     * Parameters:
     *   best_pscore_id: index of best score
     *   score: (HW, ), score prediction
     *   box_wh: (HW, 4), cxywh, bbox prediction
     *   target_pos: previous position (x & y)
     *   target_sz: previous state (w & h)
     *   scale_x: scale of cropped patch of current frame
     *   x_size: size of cropped patch
     *   penalty: scale/ratio change penalty calculated during score post-processing
     *
     * Returns:
     *   pair containing:
     *     new_target_pos: new target position
     *     new_target_sz: new target size
     */
    
    // Get prediction in crop (scale the coordinates back)
    float pred_x = box_wh.at<float>(best_pscore_id, 0) / scale_x;
    float pred_y = box_wh.at<float>(best_pscore_id, 1) / scale_x;
    float pred_w = box_wh.at<float>(best_pscore_id, 2) / scale_x;
    float pred_h = box_wh.at<float>(best_pscore_id, 3) / scale_x;
    
    // Box post-processing
    float lr = penalty[best_pscore_id] * score[best_pscore_id] * test_lr_;
    
    // Calculate new position
    float res_x = pred_x + target_pos.x - (x_size / 2.0f) / scale_x;
    float res_y = pred_y + target_pos.y - (x_size / 2.0f) / scale_x;
    
    // Update target size with learning rate
    float res_w = target_sz.width * (1.0f - lr) + pred_w * lr;
    float res_h = target_sz.height * (1.0f - lr) + pred_h * lr;
    
    // Create new position and size
    cv::Point2f new_target_pos(res_x, res_y);
    cv::Size2f new_target_sz(res_w, res_h);
    
    return {new_target_pos, new_target_sz};
}



