#pragma once

#include "include/model_manager.h"
#include "include/control_unit.h"
#include "include/detection_processor.h"
#include "include/detection_visualizer.h"
#include "include/safety_manager.h"
#include <atomic>
#include <memory>

class DetectionManager {
public:
    DetectionManager();
    
    void runDetectionLoop(ModelManager& modelManager, std::atomic<bool>& running, 
                         ControlUnit& controlUnit, int selectionStrategy, SafetyManager& safetyManager);

    // Safety manager access
    SafetyManager& getSafetyManager() { return safetyManager_; }

private:
    void processFrame(ModelManager& modelManager, ControlUnit& controlUnit, int selectionStrategy, SafetyManager& safetyManager);
    
    std::unique_ptr<DetectionProcessor> processor_;
    std::unique_ptr<DetectionVisualizer> visualizer_;
    SafetyManager safetyManager_;
}; 
