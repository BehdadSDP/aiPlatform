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

    detectionFailure_.setSelectionStrategy(selectionStrategy);
    
    cv::Rect selectedBox;
    float selectedConf;
    int selectedClassId;
    
    if (detectionFailure_.selectTarget(detections, selectedBox, selectedConf, selectedClassId)) {
        controlUnit.setDetection(selectedBox, frame, frameSeq, selectedClassId);
    } else {
        controlUnit.clearDetection();
    }
}

void DetectionProcessor::updateSimilarityReference(const cv::Mat& frame, const cv::Rect& box) {
    detectionFailure_.updateSimilarityReference(frame, box);
} 