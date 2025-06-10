#pragma once

#include "include/model_manager.h"
#include "include/control_unit.h"
#include "include/detection_processor.h"
#include "include/detection_visualizer.h"
#include <atomic>
#include <memory>

class DetectionManager {
public:
    DetectionManager();
    
    void runDetectionLoop(ModelManager& modelManager, std::atomic<bool>& running, 
                         ControlUnit& controlUnit, int selectionStrategy);

private:
    void processFrame(ModelManager& modelManager, ControlUnit& controlUnit, int selectionStrategy);
    
    std::unique_ptr<DetectionProcessor> processor_;
    std::unique_ptr<DetectionVisualizer> visualizer_;
}; 
