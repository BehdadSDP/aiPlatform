#include "include/detection_manager.h"
#include "include/frame_buffer_manager.h"
#include "include/logger.h"
#include <thread>
#include <chrono>

DetectionManager::DetectionManager(Visualizer& visualizer) : visualizer_(visualizer) {
}

void DetectionManager::runDetectionLoop(ModelManager& modelManager, std::atomic<bool>& running, 
                                       ControlUnit& controlUnit, int selectionStrategy) {
    detectionFailure_.setSelectionStrategy(selectionStrategy);
    
    while (running) {
        // Check if detection should run based on detection mode
        if (!controlUnit.waitForDetectionTurn()) {
            // Detection is sleeping - tracker is running successfully
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        
        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        cv::Mat frame = frameData.image;
        if (frame.empty()) continue;
        
        // Only run detection when waitForDetectionTurn() allows it
        // std::cout << "Detection running..." << std::endl; // Debug: Uncomment to see when detection runs
        std::vector<model::Detection> detections = modelManager.detect(frame);
        
        // Always visualize detections (whether empty or not, to show the model name)
        visualizer_.visualizeDetections(frame, detections, modelManager.getClassNames(), modelManager.getModelName());
        
        if (!detections.empty()) {
            // ✅ OPTIMIZED: Use reference to avoid copying detection data
            cv::Rect selectedBox;
            float selectedConf;
            int selectedClassId;
            
            if (detectionFailure_.selectTarget(detections, selectedBox, selectedConf, selectedClassId)) {
                // ✅ OPTIMIZED: Pass frame by reference to avoid cloning
                controlUnit.setDetection(selectedBox, frame, frameData.sequence, selectedClassId);
            }
        }
        
        // Update detection frame for combined view only (no separate detection window)
        visualizer_.updateDetectionFrame(frame);
        visualizer_.showCombinedView();
        
        // Handle tracker failure recovery
        handleTrackerFailure(controlUnit);
        
        // Small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void DetectionManager::setSelectionStrategy(int strategy) {
    detectionFailure_.setSelectionStrategy(strategy);
}

bool DetectionManager::selectTarget(const std::vector<model::Detection>& detections, 
                                    cv::Rect& selectedBox, float& selectedConf, int& selectedClassId) {
    return detectionFailure_.selectTarget(detections, selectedBox, selectedConf, selectedClassId);
}

void DetectionManager::handleTrackerFailure(ControlUnit& controlUnit) {
    if (controlUnit.hasTrackerFailed()) {
        cv::Mat lastFrame;
        cv::Rect lastBox;
        controlUnit.getFailureData(lastFrame, lastBox);
        
        // ✅ FIX: Only update similarity reference if similarity strategy is selected
        // This respects the user's selection strategy choice from config.txt
        if (detectionFailure_.getCurrentStrategyId() == DetectionFailure::SIMILARITY) {
            updateSimilarityReference(lastFrame, lastBox);
            LOG_INFO("Tracker failure detected. Updated similarity reference for similarity strategy.");
        }
         else {
            LOG_INFO("Tracker failure detected. Using configured selection strategy: {}",
                     detectionFailure_.getCurrentStrategyName());
        }
        
        // Clear the tracker failure flag after processing
        controlUnit.setTrackerFailed(false);
    }
}

void DetectionManager::processFrame(ModelManager& modelManager, ControlUnit& controlUnit, int selectionStrategy) {
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

    // Visualize detections (draws on frame but doesn't display)
    visualizer_.visualizeDetections(frame, detections, classNames, modelManager.getModelName());

    // Update detection frame for combined view only (no separate detection window)
    visualizer_.updateDetectionFrame(frame);
    visualizer_.showCombinedView();

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
