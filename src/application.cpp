#include "include/application.h"
#include "include/config_utils.h"
#include "include/resource_monitor.h"
#include "include/tracker_manager.h"
#include "include/detection_manager.h"
#include "include/frame_buffer_manager.h"
#include <iostream>
#include <sstream>
#include <csignal>
#include <thread>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <iomanip>

std::atomic<bool> Application::m_running(true);
Application::Application() : m_operationMode(0), m_showTrackingPath(false), m_selectionStrategy(0), m_mavlinkEnabled(false) {
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
        
        // Initialize frame buffer configuration
        initializeFrameBuffer();
        
        // Initialize MAVLink if enabled
        if (!initializeMAVLink()) return false;  

        // Pass MAVLink instance to NavigationUnit
        if (m_mavlink) {
            m_navigationUnit.setMavlink(m_mavlink.get());
        }
        
        // Configure navigation control parameters for smooth response
        float maxRCChangeRate = config_utils::getConfigFloat(m_config, "navigation.max_rc_change_rate");
        float filterAlpha = config_utils::getConfigFloat(m_config, "navigation.filter_alpha");
        float centeringRadius = config_utils::getConfigFloat(m_config, "navigation.centering_radius");
        m_navigationUnit.setMaxRCChangeRate(maxRCChangeRate);
        m_navigationUnit.setFilterAlpha(filterAlpha);
        m_navigationUnit.setCenteringRadius(centeringRadius);
        std::cout << "Navigation control configured: Slew rate=" << maxRCChangeRate 
                  << " PWM/sec, Filter alpha=" << filterAlpha 
                  << ", Centering radius=" << centeringRadius << "px" << std::endl;

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
/*
@brief
this is the actual function that loads the configuration from the config file
*/
bool Application::loadConfiguration(const std::string& configPath) {
    m_config = config_utils::loadConfig(configPath);
    if (m_config.empty()) {
        return false;
    }

    m_operationMode = config_utils::getConfigInt(m_config, "general.operation_mode");
    m_showTrackingPath = config_utils::getConfigInt(m_config, "visualization.show_tracking_path") == 1;
    m_selectionStrategy = config_utils::getConfigInt(m_config, "detection.selection_strategy");
    m_mavlinkEnabled = config_utils::getConfigInt(m_config, "mavlink.enabled") == 1;
    return true;
}

/*
@brief
input type: camera or video
model type: vehicle, helmet, face
operation mode: detection + tracking or detection only
selection strategy: highest confidence, upper bounding box, lower bounding box, rightmost bounding box, leftmost bounding box, similarity based
*/
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

    // Display selection strategy
    std::string strategyName;
    switch (m_selectionStrategy) {
        case 0: strategyName = "Highest Confidence"; break;
        case 1: strategyName = "Upper Bounding Box"; break;
        case 2: strategyName = "Lower Bounding Box"; break;
        case 3: strategyName = "Rightmost Bounding Box"; break;
        case 4: strategyName = "Leftmost Bounding Box"; break;
        case 5: strategyName = "Similarity Based"; break;
        default: strategyName = "Unknown"; break;
    }
    std::cout << "Selection Strategy: " << strategyName << std::endl;
    std::cout << "MAVLink: " << (m_mavlinkEnabled ? "Enabled" : "Disabled") << std::endl;
}

/*
@brief
initialize input source: camera or video 
resolution
frame rate
resolution index
*/
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
        m_cameraHandler->setRotation(config_utils::getConfigInt(m_config, "camera.rotation_angle"));
        
        // Configure exposure settings
        bool autoExposure = config_utils::getConfigInt(m_config, "camera.auto_exposure") == 1;
        if (autoExposure) {
            m_cameraHandler->setAutoExposure(true);
        } else {
            int exposureTimeUs = config_utils::getConfigInt(m_config, "camera.exposure_time_us");
            m_cameraHandler->setExposureTime(exposureTimeUs);
        }
        
        // Configure deblurring for drone vibration compensation
        bool deblurEnabled = config_utils::getConfigInt(m_config, "camera.deblur_enabled") == 1;
        if (deblurEnabled) {
            m_cameraHandler->enableDeblur(true);
            m_cameraHandler->setDeblurMethod(config_utils::getConfigInt(m_config, "camera.deblur_method"));
            m_cameraHandler->setDeblurStrength(config_utils::getConfigFloat(m_config, "camera.deblur_strength"));
        }
        
        m_cameraHandler->startStreaming();
        
        int rotationAngle = config_utils::getConfigInt(m_config, "camera.rotation_angle");
        std::cout << "Camera initialized: " << config_utils::getConfigInt(m_config, "camera.width") << "x" << config_utils::getConfigInt(m_config, "camera.height") << "@" << config_utils::getConfigFloat(m_config, "camera.frame_rate") << "fps";
        if (rotationAngle != 0) {
            std::cout << " (rotated " << rotationAngle << "°)";
        }
        if (autoExposure) {
            std::cout << " [Auto-Exposure: ON]";
        } else {
            std::cout << " [Exposure: " << config_utils::getConfigInt(m_config, "camera.exposure_time_us") << "μs]";
        }
        if (deblurEnabled) {
            std::cout << " [Deblur: ON]";
        }
        std::cout << std::endl;
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

/*
@brief
initialize models: vehicle, helmet, face
model type: yolo
model path: path to model file
model names: path to model names file
*/
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
    } else if (modelType == 3) {
        // Color detection configuration
        std::string colorsStr = config_utils::getConfigString(m_config, "color_detection.target_colors");
        
        // Parse comma-separated colors
        std::stringstream ss(colorsStr);
        std::string color;
        while (std::getline(ss, color, ',')) {
            // Trim whitespace
            color.erase(0, color.find_first_not_of(" \t"));
            color.erase(color.find_last_not_of(" \t") + 1);
            if (!color.empty()) {
                modelConfig.colorConfig.targetColors.push_back(color);
            }
        }
        
        modelConfig.colorConfig.minContourArea = config_utils::getConfigFloat(m_config, "color_detection.min_area");
        modelConfig.colorConfig.maxContourArea = config_utils::getConfigFloat(m_config, "color_detection.max_area");
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

/*
@brief
initialize tracker: vittracker, siamfc
tracker type: vittracker, siamfc
tracker path: path to tracker file
tracker names: path to tracker names file
*/
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

    auto tracker = TrackerManager::createTracker(trackerConfig);
    if (!tracker) {
        throw std::runtime_error("Failed to initialize tracker");
    }
    m_trackerManager.initialize(std::move(tracker), m_showTrackingPath);
    std::cout << "Tracker initialized successfully" << std::endl;
    return true;
}


/*
@brief
initialize frame buffer: cleanup interval
cleanup interval: interval in frames to cleanup frame buffer
*/
void Application::initializeFrameBuffer() {
    // Configure frame buffer cleanup interval from config
    int cleanupInterval = config_utils::getConfigInt(m_config, "frame_buffer.cleanup_interval");
    if (cleanupInterval > 0) {
        FrameBufferManager::getInstance().setCleanupInterval(cleanupInterval);
        std::cout << "Frame buffer cleanup interval set to: " << cleanupInterval << " frames" << std::endl;
    } else {
        std::cout << "Using default frame buffer cleanup interval: 100 frames" << std::endl;
    }
}

/*
@brief
run the application
detection thread
tracking thread
*/
void Application::run() {
    std::cout << "=== System Ready - Processing Started ===" << std::endl;
    
    // Enable combined view for detection and tracking
    m_visualizer.enableCombinedView(true);
    std::cout << "Combined Detection & Tracking view enabled" << std::endl;

    std::thread yoloThread(&Application::modelsThread, this);

    std::thread trackerThread;
    if (m_operationMode == 0) {
        m_detectionFailure.setSelectionStrategy(m_selectionStrategy);
        
        // Configure enhanced similarity parameters if similarity strategy is selected
        if (m_selectionStrategy == 5) { // Similarity strategy
            double spatialWeight = config_utils::getConfigFloat(m_config, "similarity.spatial_weight");
            double appearanceWeight = config_utils::getConfigFloat(m_config, "similarity.appearance_weight");
            double sizeWeight = config_utils::getConfigFloat(m_config, "similarity.size_weight");
            double maxMovement = config_utils::getConfigFloat(m_config, "similarity.max_expected_movement");
            double similarityThreshold = config_utils::getConfigFloat(m_config, "similarity.similarity_threshold");
            
            m_detectionFailure.configureSimilarityWeights(spatialWeight, appearanceWeight, sizeWeight, maxMovement);
            m_detectionFailure.setSimilarityThreshold(similarityThreshold);
            std::cout << "En"
                         "hanced similarity configured with threshold: " << similarityThreshold << std::endl;
        }
        
        trackerThread = std::thread(&Application::trackingThread, this);
    }

    // ✅ FIX: Cleaner thread management
    yoloThread.join();
    
    // Only join tracking thread if it was created (Mode 0)
    if (m_operationMode == 0) {
        trackerThread.join();
    }
}

void Application::modelsThread() {
    DetectionManager detectionManager(m_visualizer);
    
    // Use the public runDetectionLoop method which handles the detection loop internally
    detectionManager.runDetectionLoop(m_modelManager, m_running, m_controlUnit, m_selectionStrategy);
}

/*
@brief
tracking thread
update tracker
visualize tracking
draw safety overlays
display frame
*/
void Application::trackingThread() {
    // Static frame counter outside the loop to persist between iterations
    static int frameCounter = 0;
    
    // Create timestamped folder for this session
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&now_time_t, &tm_buf);
    
    std::ostringstream folderName;
    folderName << "/home/pi5/shared_folder/aiPlatform/images/"
               << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S");
    
    m_sessionFolder = folderName.str();
    
    // Ensure timestamped session directory exists
    std::filesystem::path sessionDir(m_sessionFolder);
    if (!std::filesystem::exists(sessionDir)) {
        std::filesystem::create_directories(sessionDir);
        std::cout << "Created session folder: " << m_sessionFolder << std::endl;
    }
    int control_test = true;
    while(m_running) {
        if (!m_controlUnit.waitForTrackingTurn()) {
            continue;
        }

        // Handle tracker initialization/re-initialization
        if (m_controlUnit.hasNewDetection()) {
            if (!m_trackerManager.isTracking() || m_controlUnit.hasTrackerFailed()) {
                cv::Rect newBox;
                cv::Mat newFrame;
                int newClassId;
                m_controlUnit.getDetectionData(newBox, newFrame, newClassId);

                if (m_trackerManager.start(newFrame, newBox, newClassId, m_modelManager.getClassNames())) {
                    m_controlUnit.setIsTracking(true);
                    m_controlUnit.setTrackerFailed(false);
                    m_controlUnit.markDetectionAsProcessed();
                }
            } else {
                m_controlUnit.markDetectionAsProcessed();
            }
        }

        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }
        cv::Mat frame = frameData.image;

        if (frame.empty()) continue;

        if (m_trackerManager.isTracking()) {
            m_trackerManager.update(frame);


            if (m_mavlink->current_flight_mode_ == 2){
            // Check if vehicle is armed from heartbeat and arm if necessary
            if (m_mavlink && m_mavlinkEnabled) {
                if (!m_mavlink->isVehicleArmed()) {
                    m_navigationUnit.armVehicle(true);
                }
            }
               // Calculate navigation error and generate control commands
               cv::Point2f rawError = m_navigationUnit.calculateError(m_trackerManager.getLastTrackBox(), frame.cols, frame.rows);
               ControlOutputs controlOutputs = m_navigationUnit.generateControlCommands(rawError, m_trackerManager.getLastTrackBox(), frame.cols, frame.rows);

            }

            if (!m_trackerManager.isTracking()) {
                m_controlUnit.setIsTracking(false);
                m_controlUnit.setTrackerFailed(true, frame, m_trackerManager.getLastTrackBox());
                std::cout << "Tracking lost - Re-enabling detection for re-initialization" << std::endl;
            }
        }

        // Process incoming MAVLink messages
        if (m_mavlink && m_mavlinkEnabled) {
            m_mavlink->processIncomingMessages();
        }

        // Visualization
        {
            std::lock_guard<std::mutex> lock(m_visMutex);
            if (m_trackerManager.isTracking()) {
                const ControlOutputs& controlOutputs = m_navigationUnit.getLastControlOutputs();
                
                // Get flight mode and MAVLink connection status
                uint32_t flightMode = 0;
                bool mavlinkConnected = false;
                if (m_mavlink && m_mavlinkEnabled) {
                    flightMode = m_mavlink->getCurrentFlightMode();
                    mavlinkConnected = m_mavlink->isConnected();
                }
                
                m_visualizer.visualizeTracking(frame, m_trackerManager.isTracking(), m_trackerManager.getLastTrackBox(),
                                             m_trackerManager.getTrackedClassId(), m_modelManager.getClassNames(),
                                             m_trackerManager.getTrackingPath(), &controlOutputs, flightMode, mavlinkConnected,
                                             m_navigationUnit.getCenteringRadius());
                
                if(true){
                    // Save the final visualized tracking frame to session folder (only when tracking)
                    std::string filename = m_sessionFolder + "/tracking_frame_" + std::to_string(frameCounter++) + ".jpg";
                    bool saved = cv::imwrite(filename, frame);
                    if (saved) {
                        std::cout << "Saved tracking frame: " << filename << std::endl;
                    } else {
                        std::cerr << "Failed to save tracking frame: " << filename << std::endl;
                    }
                }

            }
            
            // Update tracking frame for combined view only (no separate tracking window)
            m_visualizer.updateTrackingFrame(frame);
            m_visualizer.showCombinedView();
        }
    }
    cv::destroyAllWindows();
}


/*
@brief
cleanup: cleanup camera and video handler
*/
/*
@brief
Initialize MAVLink communication if enabled
*/
bool Application::initializeMAVLink() {
    if (!m_mavlinkEnabled) {
        std::cout << "MAVLink disabled in configuration" << std::endl;
        return true;
    }

    try {
        // Get MAVLink configuration
        uint8_t systemId = static_cast<uint8_t>(config_utils::getConfigInt(m_config, "mavlink.system_id"));
        uint8_t componentId = static_cast<uint8_t>(config_utils::getConfigInt(m_config, "mavlink.component_id"));
        std::string uartDevice = config_utils::getConfigString(m_config, "mavlink.uart_device");
        int baudRate = config_utils::getConfigInt(m_config, "mavlink.uart_baud_rate");

        // Create MAVLink instance
        m_mavlink = std::make_unique<Mavlink>(systemId, componentId);

        // Initialize UART
        if (!m_mavlink->initializeUART(uartDevice, baudRate)) {
            std::cerr << "Failed to initialize MAVLink UART" << std::endl;
            return false;
        }

        // Start MAVLink communication
        if (!m_mavlink->start()) {
            std::cerr << "Failed to start MAVLink communication" << std::endl;
            return false;
        }

        std::cout << "✅ MAVLink initialized successfully" << std::endl;
        std::cout << "   System ID: " << static_cast<int>(systemId) << std::endl;
        std::cout << "   Component ID: " << static_cast<int>(componentId) << std::endl;
        std::cout << "   UART: " << uartDevice << " @ " << baudRate << " baud" << std::endl;
        
        // Test message format
        m_mavlink->testMAVLinkMessageFormat();

    } catch (const std::exception& e) {
        std::cerr << "MAVLink initialization failed: " << e.what() << std::endl;
        return false;
    }

    return true;
}

void Application::cleanup() {
    if (m_mavlink) {
        m_mavlink->stop();
        m_mavlink.reset();
        std::cout << "MAVLink communication stopped" << std::endl;
    }
    
    if (m_cameraHandler) {
        m_cameraHandler->cleanup();
    }
    if (m_videoHandler) {
        m_videoHandler->cleanup();
    }
} 



