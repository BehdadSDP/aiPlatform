#include "include/camera_handler.h"
#include "include/video_handler.h"
#include "include/model_manager.h"
#include "include/control_unit.h"
#include "include/config_utils.h"
#include "include/resource_monitor.h"
#include "include/tracker_factory.h"
#include "include/tracker_manager.h"
#include "include/detection_manager.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <csignal>
#include <memory>
#include <fstream>
#include <vector>
#include <filesystem>

std::atomic<bool>* g_running = nullptr;

void signalHandler(int) {
    if (g_running) g_running->store(false);
}





// Simple wrapper function for the detection thread
void threadYolo(ModelManager &modelManager, std::atomic<bool> &running, ControlUnit& controlUnit, int selectionStrategy) {
    // Create detection manager and run the detection loop
    DetectionManager detectionManager;
    detectionManager.runDetectionLoop(modelManager, running, controlUnit, selectionStrategy);
}




// Simple wrapper function for the tracker thread
void threadTracker(std::atomic<bool> &running, std::unique_ptr<TrackerInterface>& tracker,
                  ModelManager& modelManager, ControlUnit& controlUnit) {
    // Create tracker manager and run the tracking loop
    TrackerManager trackerManager(std::move(tracker));
    trackerManager.runTrackingLoop(running, modelManager, controlUnit);
}


int main() {
    try {
        // Ensure logs directory exists with proper permissions
        std::filesystem::path logsDir("logs");
        if (!std::filesystem::exists(logsDir)) {
            std::filesystem::create_directories(logsDir);
            std::filesystem::permissions(logsDir, 
                std::filesystem::perms::owner_all | 
                std::filesystem::perms::group_read | 
                std::filesystem::perms::others_read);
        }

        std::atomic<bool> running(true);
        g_running = &running;
        std::signal(SIGINT, signalHandler);

        // Start resource monitoring with absolute path
        std::string logPath = std::filesystem::absolute(logsDir / "resource_usage.csv").string();
        std::cout << "Starting resource monitoring, log file: " << logPath << std::endl;
        ResourceMonitor::getInstance().startMonitoring(logPath, 30); // Log every 30 seconds

        ControlUnit controlUnit;
        
        // Config related code
        auto config = config_utils::loadConfig("/home/pi5/shared_folder/aiPlatform/config/config.txt");
        
        // Get input type configuration
        int inputType = config_utils::getConfigInt(config, "input.input_type");
        
        // Camera settings (used when input_type = 0)
        int resolutionIndex = config_utils::getConfigInt(config, "camera.resolution_index");
        int customWidth = config_utils::getConfigInt(config, "camera.width");
        int customHeight = config_utils::getConfigInt(config, "camera.height");
        float frameRate = config_utils::getConfigFloat(config, "camera.frame_rate");
        int detectionInterval = config_utils::getConfigInt(config, "detection.interval");
        int trackingInterval = config_utils::getConfigInt(config, "tracking.interval");
        int detectionMode = config_utils::getConfigInt(config, "detection.mode");
        int targetClassId = config_utils::getConfigInt(config, "general.target_class_id");
        int trackerType = config_utils::getConfigInt(config, "tracking.tracker_type");
        int selectionStrategy = config_utils::getConfigInt(config, "detection.selection_strategy");
        int modelType = config_utils::getConfigInt(config, "detection_model.model_type");

        // Read file paths from config based on model type
        std::string yoloModelPath, classNamesPath;
        if (modelType == 0) {
            // COCO general detection
            yoloModelPath = config_utils::getConfigString(config, "detection_model.yolo_model_path");
            classNamesPath = config_utils::getConfigString(config, "detection_model.coco_names_path");
        } else if (modelType == 1) {
            // Helmet detection
            yoloModelPath = config_utils::getConfigString(config, "detection_model.helmet_model_path");
            classNamesPath = config_utils::getConfigString(config, "detection_model.helmet_names_path");
        } else {
            throw std::runtime_error("Invalid model type: " + std::to_string(modelType));
        }
        
        std::string vitTrackerModelPath = config_utils::getConfigString(config, "detection_model.vittracker_model_path");
        std::string siamfcFeatureModelPath = config_utils::getConfigString(config, "detection_model.siamfc_feature_model_path");
        std::string siamfcTrackingModelPath = config_utils::getConfigString(config, "detection_model.siamfc_tracking_model_path");

        // Initialize input source based on configuration
        std::unique_ptr<CameraHandler> cameraHandler;
        std::unique_ptr<VideoHandler> videoHandler;
        
        if (inputType == 0) {
            // Camera input
            std::cout << "Using camera input" << std::endl;
            cameraHandler = std::make_unique<CameraHandler>(controlUnit);
            cameraHandler->initialize();
            cameraHandler->acquireCamera();
            cameraHandler->configureCamera(resolutionIndex, customWidth, customHeight);
            cameraHandler->setFrameRate(frameRate);
            cameraHandler->startStreaming();
        } else if (inputType == 1) {
            // Video input
            std::cout << "Using video input" << std::endl;
            std::string videoPath = config_utils::getConfigString(config, "input.video_path");
            videoHandler = std::make_unique<VideoHandler>(controlUnit);
            videoHandler->initialize(videoPath);
            videoHandler->startStreaming();
        } else {
            throw std::runtime_error("Invalid input type: " + std::to_string(inputType) + ". Use 0 for camera, 1 for video.");
        }

        // Configure ControlUnit
        controlUnit.setDetectionMode(detectionMode);
        controlUnit.setDetectionInterval(detectionInterval);
        controlUnit.setTrackingInterval(trackingInterval);

        // Create and initialize ModelManager
        ModelManager modelManager;
        ModelConfig modelConfig;
        modelConfig.type = static_cast<ModelType>(modelType);
        modelConfig.modelPath = yoloModelPath;
        modelConfig.classNamesPath = classNamesPath;
        modelConfig.targetClassId = targetClassId;
        
        if (!modelManager.initialize(modelConfig)) {
            throw std::runtime_error("Failed to initialize ModelManager");
        }

        // Set up the appropriate tracker using factory
        TrackerConfig trackerConfig;
        trackerConfig.type = static_cast<TrackerType>(trackerType);
        trackerConfig.vitModelPath = vitTrackerModelPath;
        trackerConfig.siamfcFeatureModelPath = siamfcFeatureModelPath;
        trackerConfig.siamfcTrackingModelPath = siamfcTrackingModelPath;
        
        std::unique_ptr<TrackerInterface> tracker = TrackerFactory::createTracker(trackerConfig);

        // Print starting message
        std::cout << "Streaming... Press Ctrl+C to exit." << std::endl;

        // Start worker threads
        std::thread yoloThread(threadYolo, std::ref(modelManager), std::ref(running),
                              std::ref(controlUnit), selectionStrategy);

        std::thread trackerThread(threadTracker, std::ref(running), std::ref(tracker),
                                 std::ref(modelManager), std::ref(controlUnit));

        // Join threads when done
        yoloThread.join();
        trackerThread.join();

        // Clean up input sources
        if (cameraHandler) {
            cameraHandler->cleanup();
        }
        if (videoHandler) {
            videoHandler->cleanup();
        }
        std::cout << "Cleanup complete." << std::endl;

        // Cleanup
        ResourceMonitor::getInstance().stopMonitoring();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        ResourceMonitor::getInstance().stopMonitoring();
        return 1;
    }
}
