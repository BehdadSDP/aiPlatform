#pragma once

#include "include/model_manager.h"
#include "include/control_unit.h"
#include "include/detection_processor.h"
#include "include/visualizer.h"
#include "include/safety_manager.h"
#include <atomic>
#include <memory>

class DetectionManager {
public:
    DetectionManager(Visualizer& visualizer);
    
    void runDetectionLoop(ModelManager& modelManager, std::atomic<bool>& running, 
                         ControlUnit& controlUnit, int selectionStrategy, SafetyManager& safetyManager);

private:
    void processFrame(ModelManager& modelManager, ControlUnit& controlUnit, int selectionStrategy, SafetyManager& safetyManager);
    void handleTrackerFailure(ControlUnit& controlUnit);
    
    std::unique_ptr<DetectionProcessor> processor_;
    Visualizer& visualizer_;
}; 
