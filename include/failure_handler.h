#pragma once

#include "include/model.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>

class DetectionFailure {
public:
    // Strategy types
    enum Strategy {
        HIGHEST_CONFIDENCE = 0,
        UPPER_BOX = 1,
        LOWER_BOX = 2,
        RIGHTMOST_BOX = 3,
        LEFTMOST_BOX = 4,
        SIMILARITY = 5,
        MANUAL_SELECTION = 6
    };

    DetectionFailure();
    
    // Target selection
    void setSelectionStrategy(int strategyId);
    bool selectTarget(const std::vector<model::Detection>& detections,
                      cv::Rect& selectedBox,
                      float& selectedConf,
                      int& selectedClassId);

    // Failure recovery
    bool areObjectsSimilar(const cv::Mat& lastFrame, const cv::Rect& lastBox,
                           const cv::Mat& newFrame, const cv::Rect& newBox,
                           double similarityThreshold = 0.7) const;
    
    // Update reference for similarity strategy
    void updateSimilarityReference(const cv::Mat& frame, const cv::Rect& box);
    
    // Configure enhanced similarity parameters
    void configureSimilarityWeights(double spatialWeight, double appearanceWeight, double sizeWeight, double maxMovement = 100.0);
    
    // Set similarity threshold
    void setSimilarityThreshold(double threshold);
    
    // Get current strategy name for debugging
    std::string getCurrentStrategyName() const;
    
    // Get current strategy ID
    int getCurrentStrategyId() const;
    
    // Get strategy name by ID
    static std::string getStrategyName(int strategyId);
    
    // Reset manual selection state (called when tracker fails)
    void resetManualSelectionState();

private:
    // Selection strategy implementations
    bool selectHighestConfidence(const std::vector<model::Detection>& detections,
                                cv::Rect& selectedBox, float& selectedConf, int& selectedClassId);
    
    bool selectUpperBox(const std::vector<model::Detection>& detections,
                       cv::Rect& selectedBox, float& selectedConf, int& selectedClassId);
    
    bool selectLowerBox(const std::vector<model::Detection>& detections,
                       cv::Rect& selectedBox, float& selectedConf, int& selectedClassId);
    
    bool selectRightmostBox(const std::vector<model::Detection>& detections,
                           cv::Rect& selectedBox, float& selectedConf, int& selectedClassId);
    
    bool selectLeftmostBox(const std::vector<model::Detection>& detections,
                          cv::Rect& selectedBox, float& selectedConf, int& selectedClassId);
    
    bool selectSimilarity(const std::vector<model::Detection>& detections,
                         cv::Rect& selectedBox, float& selectedConf, int& selectedClassId);
    
    bool selectManual(const std::vector<model::Detection>& detections,
                     cv::Rect& selectedBox, float& selectedConf, int& selectedClassId);
    
    // Non-blocking manual selection methods
    bool hasManualSelection() const;
    bool getManualSelection(cv::Rect& selectedBox, float& selectedConf, int& selectedClassId);
    void processManualSelection(const std::vector<model::Detection>& detections);
    void setManualSelection(int choice);
    
    // Helper methods
    double calculateSimilarity(const cv::Mat& frame, const cv::Rect& box) const;
    
    // Enhanced similarity methods for tracking scenarios
    double calculateEnhancedSimilarity(const cv::Mat& frame, const cv::Rect& box) const;
    double calculateSpatialSimilarity(const cv::Rect& box) const;
    double calculateAppearanceSimilarity(const cv::Mat& frame, const cv::Rect& box) const;
    double calculateSizeSimilarity(const cv::Rect& box) const;
    
    // Member variables
    int selectedStrategyId_;
    cv::Mat lastFailureFrame_;
    cv::Rect lastFailureBox_;
    double similarityThreshold_;
    
    // Enhanced similarity parameters
    double spatialWeight_;
    double appearanceWeight_;
    double sizeWeight_;
    double maxExpectedMovement_;  // Maximum expected movement in pixels
    
    // Manual selection state
    std::vector<model::Detection> pendingDetections_;
    bool hasPendingSelection_;
    int selectedChoice_;
    cv::Rect selectedBox_;
    float selectedConf_;
    int selectedClassId_;
}; 