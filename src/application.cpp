#include "include/application.h"
#include "include/config_utils.h"
#include "include/resource_monitor.h"
#include "include/tracker_factory.h"
#include "include/detection_manager.h"
#include "include/frame_buffer_manager.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <filesystem>
#include <opencv2/opencv.hpp>

std::atomic<bool> Application::m_running(true);

Application::Application() : m_operationMode(0), m_showTrackingPath(false), m_selectionStrategy(0) {
    setupSignalHandler();
}

Application::~Application() {
    cleanup();
    ResourceMonitor::getInstance().stopMonitoring();
    std::cout << "=== System Shutdown Complete ===" << std::endl;
}

void Application::setupSignalHandler() {
    std::signal(SIGINT, Application::signalHandler);
}

void Application::signalHandler(int) {
    m_running.store(false);
}

bool Application::initialize(const std::string& configPath) {
    try {
        // Ensure logs directory exists
        std::filesystem::path logsDir("logs");
        if (!std::filesystem::exists(logsDir)) {
            std::filesystem::create_directories(logsDir);
            std::filesystem::permissions(logsDir,
                std::filesystem::perms::owner_all |
                std::filesystem::perms::group_read |
                std::filesystem::perms::others_read);
        }

        std::string logPath = std::filesystem::absolute(logsDir / "resource_usage.csv").string();
        ResourceMonitor::getInstance().startMonitoring(logPath, 30);

        if (!loadConfiguration(configPath)) return false;
        logConfiguration();
        if (!initializeInputSource()) return false;
        if (!initializeModels()) return false;
        if (!initializeTracker()) return false;
        
        //initializeSafetyManager();

        m_controlUnit.setDetectionMode(config_utils::getConfigInt(m_config, "detection.mode"));
        m_controlUnit.setDetectionInterval(config_utils::getConfigInt(m_config, "detection.interval"));
        m_controlUnit.setTrackingInterval(config_utils::getConfigInt(m_config, "tracking.interval"));
        m_controlUnit.setOperationMode(m_operationMode);

    } catch (const std::exception& e) {
        std::cerr << "Initialization failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool Application::loadConfiguration(const std::string& configPath) {
    m_config = config_utils::loadConfig(configPath);
    if (m_config.empty()) {
        return false;
    }

    m_operationMode = config_utils::getConfigInt(m_config, "general.operation_mode");
    m_showTrackingPath = config_utils::getConfigInt(m_config, "visualization.show_tracking_path") == 1;
    m_selectionStrategy = config_utils::getConfigInt(m_config, "detection.selection_strategy");
    return true;
}

void Application::logConfiguration() const {
    std::cout << "=== AI Platform Starting ===" << std::endl;
    std::cout << "Input: " << (config_utils::getConfigInt(m_config, "input.input_type") == 0 ? "Camera" : "Video") << std::endl;
    
    int modelType = config_utils::getConfigInt(m_config, "detection_model.model_type");
    std::string modelTypeName = (modelType == 0) ? "Vehicle Detection" :
                               (modelType == 1) ? "Helmet Detection" :
                               (modelType == 2) ? "Face Detection" : "Unknown";
    std::cout << "Model: " << modelTypeName << std::endl;
    
    std::cout << "Operation Mode: " << (m_operationMode == 0 ? "Detection + Tracking" : "Detection Only") << std::endl;
    if (m_operationMode == 0) {
        int trackerType = config_utils::getConfigInt(m_config, "tracking.tracker_type");
        std::cout << "Tracker: " << (trackerType == 0 ? "VitTracker" : "SiamFCPP") << std::endl;
        std::cout << "Path Visualization: " << (m_showTrackingPath ? "Enabled" : "Disabled") << std::endl;
    }
    
    std::cout << "Detection Mode: " << (config_utils::getConfigInt(m_config, "detection.mode") == 0 ? "Interval" : "Continuous") << std::endl;

    bool trafficIntensityEnabled = config_utils::getConfigInt(m_config, "traffic_intensity.enabled") == 1;
    std::cout << "Traffic Intensity: " << (trafficIntensityEnabled ? "Enabled" : "Disabled") << std::endl;
}

bool Application::initializeInputSource() {
    int inputType = config_utils::getConfigInt(m_config, "input.input_type");
    if (inputType == 0) {
        m_cameraHandler = std::make_unique<CameraHandler>(m_controlUnit);
        m_cameraHandler->initialize();
        m_cameraHandler->acquireCamera();
        m_cameraHandler->configureCamera(config_utils::getConfigInt(m_config, "camera.resolution_index"),
                                       config_utils::getConfigInt(m_config, "camera.width"),
                                       config_utils::getConfigInt(m_config, "camera.height"));
        m_cameraHandler->setFrameRate(config_utils::getConfigFloat(m_config, "camera.frame_rate"));
        m_cameraHandler->startStreaming();
        std::cout << "Camera initialized: " << config_utils::getConfigInt(m_config, "camera.width") << "x" << config_utils::getConfigInt(m_config, "camera.height") << "@" << config_utils::getConfigFloat(m_config, "camera.frame_rate") << "fps" << std::endl;
    } else if (inputType == 1) {
        std::string videoPath = config_utils::getConfigString(m_config, "input.video_path");
        m_videoHandler = std::make_unique<VideoHandler>(m_controlUnit);
        m_videoHandler->initialize(videoPath);
        m_videoHandler->startStreaming();
        std::cout << "Video source initialized: " << videoPath << std::endl;
    } else {
        throw std::runtime_error("Invalid input type: " + std::to_string(inputType));
    }
    return true;
}

bool Application::initializeModels() {
    ModelConfig modelConfig;
    int modelType = config_utils::getConfigInt(m_config, "detection_model.model_type");
    modelConfig.type = static_cast<ModelType>(modelType);
    
    if (modelType == 0) {
        modelConfig.modelPath = config_utils::getConfigString(m_config, "detection_model.yolo_model_path");
        modelConfig.classNamesPath = config_utils::getConfigString(m_config, "detection_model.coco_names_path");
    } else if (modelType == 1) {
        modelConfig.modelPath = config_utils::getConfigString(m_config, "detection_model.helmet_model_path");
        modelConfig.classNamesPath = config_utils::getConfigString(m_config, "detection_model.helmet_names_path");
    } else if (modelType == 2) {
        modelConfig.modelPath = config_utils::getConfigString(m_config, "detection_model.face_model_path");
        modelConfig.classNamesPath = config_utils::getConfigString(m_config, "detection_model.face_names_path");
    } else {
        throw std::runtime_error("Invalid model type: " + std::to_string(modelType));
    }

    modelConfig.targetClassId = config_utils::getConfigInt(m_config, "general.target_class_id");

    if (!m_modelManager.initialize(modelConfig)) {
        throw std::runtime_error("Failed to initialize detection model");
    }
    std::cout << "Detection model loaded successfully" << std::endl;
    return true;
}

bool Application::initializeTracker() {
    if (m_operationMode != 0) {
        std::cout << "Detection-only mode: Tracker disabled" << std::endl;
        return true;
    }

    TrackerConfig trackerConfig;
    trackerConfig.type = static_cast<TrackerType>(config_utils::getConfigInt(m_config, "tracking.tracker_type"));
    trackerConfig.vitModelPath = config_utils::getConfigString(m_config, "detection_model.vittracker_model_path");
    trackerConfig.siamfcFeatureModelPath = config_utils::getConfigString(m_config, "detection_model.siamfc_feature_model_path");
    trackerConfig.siamfcTrackingModelPath = config_utils::getConfigString(m_config, "detection_model.siamfc_tracking_model_path");

    m_tracker = TrackerFactory::createTracker(trackerConfig);
    if (!m_tracker) {
        throw std::runtime_error("Failed to initialize tracker");
    }
    std::cout << "Tracker initialized successfully" << std::endl;
    return true;
}

void Application::initializeSafetyManager() {
    m_safetyManager.loadHazardZones(m_config);
    m_safetyManager.loadTrafficIntensity(m_config);
}

void Application::run() {
    std::cout << "=== System Ready - Processing Started ===" << std::endl;

    m_controlUnit.initializeTracker(std::move(m_tracker), m_showTrackingPath);

    std::thread yoloThread(&Application::detectionThread, this);

    std::thread trackerThread;
    if (m_operationMode == 0) {
        trackerThread = std::thread(&Application::trackingThread, this);
    }

    yoloThread.join();
    if (m_operationMode == 0 && trackerThread.joinable()) {
        trackerThread.join();
    }
}

void Application::detectionThread() {
    DetectionManager detectionManager(m_visualizer);
    detectionManager.runDetectionLoop(m_modelManager, m_running, m_controlUnit, m_selectionStrategy, m_safetyManager);
}

void Application::trackingThread() {
    while(m_running) {
        if (!m_controlUnit.waitForTrackingTurn()) {
            continue;
        }

        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) continue;

        if (m_controlUnit.isTracking()) {
            m_controlUnit.updateTracker(frame);
        }

        // Visualization
        {
            std::lock_guard<std::mutex> lock(m_visMutex);
            if (m_controlUnit.isTracking()) {
                m_visualizer.visualizeTracking(frame, m_controlUnit.isTracking(), m_controlUnit.getLastTrackBox(),
                                             m_controlUnit.getTrackedClassId(), m_modelManager.getClassNames(),
                                             m_controlUnit.getTrackingPath());
            }
            m_safetyManager.drawSafetyOverlays(frame);
            m_visualizer.displayFrame(frame, "Tracking View");
        }
    }
    cv::destroyAllWindows();
}

void Application::cleanup() {
    if (m_cameraHandler) {
        m_cameraHandler->cleanup();
    }
    if (m_videoHandler) {
        m_videoHandler->cleanup();
    }
} 