#include "include/detection_processor.h"

DetectionProcessor::DetectionProcessor() {}

void DetectionProcessor::processDetections(const std::vector<model::Detection>& detections, 
                                          const cv::Mat& frame, uint64_t frameSeq, 
                                          ControlUnit& controlUnit, int selectionStrategy) {
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