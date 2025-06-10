#include "include/detection_manager.h"
#include "include/frame_buffer_manager.h"

DetectionManager::DetectionManager() 
    : processor_(std::make_unique<DetectionProcessor>()),
      visualizer_(std::make_unique<DetectionVisualizer>()) {}

void DetectionManager::runDetectionLoop(ModelManager& modelManager, std::atomic<bool>& running, 
                                       ControlUnit& controlUnit, int selectionStrategy) {
    while (running) {
        // Wait for our turn to run detection
        if (!controlUnit.waitForDetectionTurn()) {
            continue;
        }

        processFrame(modelManager, controlUnit, selectionStrategy);
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

    // Visualize detections
    visualizer_->visualizeDetections(frame, detections, classNames);

    // Process detections for tracking
    processor_->processDetections(detections, frame, frameData.sequence, controlUnit, selectionStrategy);
} 