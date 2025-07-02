#include "include/detection_manager.h"
#include "include/frame_buffer_manager.h"

DetectionManager::DetectionManager(Visualizer& visualizer) 
    : visualizer_(visualizer) {}

void DetectionManager::runDetectionLoop(ModelManager& modelManager, std::atomic<bool>& running, 
                                       ControlUnit& controlUnit, int selectionStrategy, SafetyManager& safetyManager) {
    while (running) {
        // Wait for our turn to run detection
        if (!controlUnit.waitForDetectionTurn()) {
            continue;
        }

        // Check for tracker failures and update reference
        handleTrackerFailure(controlUnit);

        processFrame(modelManager, controlUnit, selectionStrategy, safetyManager);
    }
}

void DetectionManager::handleTrackerFailure(ControlUnit& controlUnit) {
    if (controlUnit.hasTrackerFailed()) {
        cv::Mat lastFrame;
        cv::Rect lastBox;
        controlUnit.getFailureData(lastFrame, lastBox);
        
        // Update the detection manager with the failure reference
        updateSimilarityReference(lastFrame, lastBox);
        
        std::cout << "Tracker failure detected. Updated similarity reference." << std::endl;
        
        // Clear the tracker failure flag after processing
        controlUnit.setTrackerFailed(false);
    }
}

void DetectionManager::processFrame(ModelManager& modelManager, ControlUnit& controlUnit, int selectionStrategy, SafetyManager& safetyManager) {
    // Get the latest frame
    FrameData frameData;
    if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
        return;
    }

    cv::Mat frame = frameData.image;
    if (frame.empty()) return;

    // Run detection
    std::vector<model::Detection> detections = modelManager.detect(frame);

    // Get class names from model manager
    const std::vector<std::string>& classNames = modelManager.getClassNames();

    // Process safety monitoring (hazard zones and traffic intensity)
    safetyManager.processDetections(detections, classNames);

    // Visualize detections (draws on frame but doesn't display)
    // Use the new method that considers traffic intensity polygons
    visualizer_.visualizeDetections(frame, detections, classNames, safetyManager.getTrafficIntensityManager());
    
    // Draw safety overlays (hazard zones and traffic intensity)
    safetyManager.drawSafetyOverlays(frame);

    // Now display the complete frame with all overlays
    visualizer_.displayFrame(frame);

    // Process detections for tracking (merged functionality)
    processDetections(detections, frame, frameData.sequence, controlUnit, selectionStrategy);
}

// Merged DetectionProcessor functionality
void DetectionManager::processDetections(const std::vector<model::Detection>& detections, 
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

void DetectionManager::updateSimilarityReference(const cv::Mat& frame, const cv::Rect& box) {
    detectionFailure_.updateSimilarityReference(frame, box);
} 