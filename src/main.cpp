#include "include/camera_handler.h"
#include "include/video_handler.h"
#include "include/frame_buffer_manager.h"
#include "include/model.h"
#include "include/model_manager.h"
#include "include/control_unit.h"
#include "include/config_utils.h"
#include "include/selection_strategy.h"
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
    DetectionManager detectionManager;
    detectionManager.runDetectionLoop(modelManager, running, controlUnit, selectionStrategy);
}

// Simple wrapper function for the tracker thread
void threadTracker(std::atomic<bool> &running, std::unique_ptr<TrackerInterface>& tracker,
                  ModelManager& modelManager, ControlUnit& controlUnit, bool showTrackingPath) {
    TrackerManager trackerManager(std::move(tracker), showTrackingPath);
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

        // Start resource monitoring
        std::string logPath = std::filesystem::absolute(logsDir / "resource_usage.csv").string();
        ResourceMonitor::getInstance().startMonitoring(logPath, 30);

        ControlUnit controlUnit;
        
        // Load configuration
        auto config = config_utils::loadConfig("/home/pi5/shared_folder/aiPlatform/config/config.txt");
        
        // Read configuration values
        int inputType = config_utils::getConfigInt(config, "input.input_type");
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
        bool showTrackingPath = config_utils::getConfigInt(config, "visualization.show_tracking_path") == 1;

        // Log system configuration
        std::cout << "=== AI Platform Starting ===" << std::endl;
        std::cout << "Input: " << (inputType == 0 ? "Camera" : "Video") << std::endl;
        std::cout << "Model: " << (modelType == 0 ? "COCO Detection" : "Helmet Detection") << std::endl;
        std::cout << "Tracker: " << (trackerType == 0 ? "VitTracker" : "SiamFCPP") << std::endl;
        std::cout << "Detection Mode: " << (detectionMode == 0 ? "Interval" : "Continuous") << std::endl;
        std::cout << "Path Visualization: " << (showTrackingPath ? "Enabled" : "Disabled") << std::endl;

        // Read model paths
        std::string yoloModelPath, classNamesPath;
        if (modelType == 0) {
            yoloModelPath = config_utils::getConfigString(config, "detection_model.yolo_model_path");
            classNamesPath = config_utils::getConfigString(config, "detection_model.coco_names_path");
        } else if (modelType == 1) {
            yoloModelPath = config_utils::getConfigString(config, "detection_model.helmet_model_path");
            classNamesPath = config_utils::getConfigString(config, "detection_model.helmet_names_path");
        } else {
            throw std::runtime_error("Invalid model type: " + std::to_string(modelType));
        }
        
        std::string vitTrackerModelPath = config_utils::getConfigString(config, "detection_model.vittracker_model_path");
        std::string siamfcFeatureModelPath = config_utils::getConfigString(config, "detection_model.siamfc_feature_model_path");
        std::string siamfcTrackingModelPath = config_utils::getConfigString(config, "detection_model.siamfc_tracking_model_path");

        // Initialize input source
        std::unique_ptr<CameraHandler> cameraHandler;
        std::unique_ptr<VideoHandler> videoHandler;
        
        if (inputType == 0) {
            cameraHandler = std::make_unique<CameraHandler>(controlUnit);
            cameraHandler->initialize();
            cameraHandler->acquireCamera();
            cameraHandler->configureCamera(resolutionIndex, customWidth, customHeight);
            cameraHandler->setFrameRate(frameRate);
            cameraHandler->startStreaming();
            std::cout << "Camera initialized: " << customWidth << "x" << customHeight << "@" << frameRate << "fps" << std::endl;
        } else if (inputType == 1) {
            std::string videoPath = config_utils::getConfigString(config, "input.video_path");
            videoHandler = std::make_unique<VideoHandler>(controlUnit);
            videoHandler->initialize(videoPath);
            videoHandler->startStreaming();
            std::cout << "Video source initialized: " << videoPath << std::endl;
        } else {
            throw std::runtime_error("Invalid input type: " + std::to_string(inputType));
        }

        // Configure control unit
        controlUnit.setDetectionMode(detectionMode);
        controlUnit.setDetectionInterval(detectionInterval);
        controlUnit.setTrackingInterval(trackingInterval);

        // Initialize detection model
        ModelManager modelManager;
        ModelConfig modelConfig;
        modelConfig.type = static_cast<ModelType>(modelType);
        modelConfig.modelPath = yoloModelPath;
        modelConfig.classNamesPath = classNamesPath;
        modelConfig.targetClassId = targetClassId;
        
        if (!modelManager.initialize(modelConfig)) {
            throw std::runtime_error("Failed to initialize detection model");
        }
        std::cout << "Detection model loaded successfully" << std::endl;

        // Initialize tracker
        TrackerConfig trackerConfig;
        trackerConfig.type = static_cast<TrackerType>(trackerType);
        trackerConfig.vitModelPath = vitTrackerModelPath;
        trackerConfig.siamfcFeatureModelPath = siamfcFeatureModelPath;
        trackerConfig.siamfcTrackingModelPath = siamfcTrackingModelPath;
        
        std::unique_ptr<TrackerInterface> tracker = TrackerFactory::createTracker(trackerConfig);
        std::cout << "Tracker initialized successfully" << std::endl;

        // Start processing
        std::cout << "=== System Ready - Processing Started ===" << std::endl;

        // Start worker threads
        std::thread yoloThread(threadYolo, std::ref(modelManager), std::ref(running),
                              std::ref(controlUnit), selectionStrategy);

        std::thread trackerThread(threadTracker, std::ref(running), std::ref(tracker),
                                 std::ref(modelManager), std::ref(controlUnit), showTrackingPath);

        // Wait for threads
        yoloThread.join();
        trackerThread.join();

        // Cleanup
        if (cameraHandler) {
            cameraHandler->cleanup();
        }
        if (videoHandler) {
            videoHandler->cleanup();
        }
        
        ResourceMonitor::getInstance().stopMonitoring();
        std::cout << "=== System Shutdown Complete ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        ResourceMonitor::getInstance().stopMonitoring();
        return 1;
    }
}
