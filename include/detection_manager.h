#pragma once

#include "include/model_manager.h"
#include "include/control_unit.h"
#include "include/failure_handler.h"
#include "include/visualizer.h"
#include <atomic>
#include <memory>

class DetectionManager {
public:
    DetectionManager(Visualizer& visualizer);
    
    void runDetectionLoop(ModelManager& modelManager, std::atomic<bool>& running, 
                         ControlUnit& controlUnit, int selectionStrategy);

private:
    void processFrame(ModelManager& modelManager, ControlUnit& controlUnit, int selectionStrategy);
    void handleTrackerFailure(ControlUnit& controlUnit);
    
    // Merged DetectionProcessor functionality
    void processDetections(const std::vector<model::Detection>& detections, 
                          const cv::Mat& frame, uint64_t frameSeq, 
                          ControlUnit& controlUnit, int selectionStrategy);
    void updateSimilarityReference(const cv::Mat& frame, const cv::Rect& box);
    
    DetectionFailure detectionFailure_;
    Visualizer& visualizer_;
}; 
