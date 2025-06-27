#include "include/detection_processor.h"

DetectionProcessor::DetectionProcessor() {}

void DetectionProcessor::processDetections(const std::vector<model::Detection>& detections, 
                                          const cv::Mat& frame, uint64_t frameSeq, 
                                          ControlUnit& controlUnit, int selectionStrategy) {
    // In detection-only mode, we don't need to pass data to tracking
    if (controlUnit.isDetectionOnly()) {
        // Just return - detections are already visualized by DetectionVisualizer
        return;
    }

    if (detections.empty()) {
        controlUnit.clearDetection();
        return;
    }

    // Create strategy using factory
    auto strategy = SelectionStrategyFactory::createStrategy(selectionStrategy);
    
    cv::Rect selectedBox;
    float selectedConf;
    int selectedClassId;
    
    // Use strategy to select detection
    if (strategy->selectDetection(detections, selectedBox, selectedConf, selectedClassId)) {
        controlUnit.setDetection(selectedBox, frame, frameSeq, selectedClassId);
    } else {
        controlUnit.clearDetection();
    }
} 