#include "include/detection_manager.h"
#include "include/frame_buffer_manager.h"

DetectionManager::DetectionManager() 
    : processor_(std::make_unique<DetectionProcessor>()),
      visualizer_(std::make_unique<DetectionVisualizer>()) {}

void DetectionManager::runDetectionLoop(ModelManager& modelManager, std::atomic<bool>& running, 
                                       ControlUnit& controlUnit, int selectionStrategy, SafetyManager& safetyManager) {
    while (running) {
        // Wait for our turn to run detection
        if (!controlUnit.waitForDetectionTurn()) {
            continue;
        }

        processFrame(modelManager, controlUnit, selectionStrategy, safetyManager);
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
    visualizer_->visualizeDetections(frame, detections, classNames, safetyManager.getTrafficIntensityManager());
    
    // Draw safety overlays (hazard zones and traffic intensity)
    safetyManager.drawSafetyOverlays(frame);

    // Now display the complete frame with all overlays
    static bool windowCreated = false;
    if (!windowCreated) {
        cv::namedWindow("Detection View", cv::WINDOW_NORMAL);
        cv::resizeWindow("Detection View", 800, 600);
        windowCreated = true;
    }
    cv::imshow("Detection View", frame);
    cv::waitKey(1);

    // Process detections for tracking
    processor_->processDetections(detections, frame, frameData.sequence, controlUnit, selectionStrategy);
} 