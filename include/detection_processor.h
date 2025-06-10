#pragma once

#include "include/model.h"
#include "control_unit.h"
#include "selection_strategy.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>

class DetectionProcessor {
public:
    DetectionProcessor();
    
    void processDetections(const std::vector<model::Detection>& detections, 
                          const cv::Mat& frame, uint64_t frameSeq, 
                          ControlUnit& controlUnit, int selectionStrategy);

private:
    std::unique_ptr<SelectionStrategy> createSelectionStrategy(int strategy);
}; 
