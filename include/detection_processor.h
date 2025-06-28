#pragma once

#include "include/model.h"
#include "include/control_unit.h"
#include "include/failure_handler.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>

class DetectionProcessor {
public:
    DetectionProcessor();
    
    void processDetections(const std::vector<model::Detection>& detections, 
                          const cv::Mat& frame, uint64_t frameSeq, 
                          ControlUnit& controlUnit, int selectionStrategy);
    
    // Update reference for similarity strategy
    void updateSimilarityReference(const cv::Mat& frame, const cv::Rect& box);

private:
    DetectionFailure detectionFailure_;
}; 
