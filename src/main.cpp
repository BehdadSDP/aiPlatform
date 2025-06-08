#include "include/camera_handler.h"
#include "include/video_handler.h"
#include "include/frame_buffer_manager.h"
#include "include/model.h"
#include "include/model_manager.h"
#include "include/vittracker.h"
#include "include/siamfc_pp_tracker.h"
#include "include/control_unit.h"
#include "include/config_utils.h"
#include "include/selection_strategy.h"
#include "include/resource_monitor.h"
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

// Updated to use SelectionStrategy
void processDetections(const std::vector<model::Detection>& detections, const cv::Mat& frame, 
                    uint64_t frameSeq, ControlUnit& controlUnit, int selectionStrategy) {
    if (detections.empty()) {
        controlUnit.clearDetection();
        return;
    }

    // Create strategy using factory
    auto strategy = SelectionStrategyFactory::createStrategy(selectionStrategy);
    
    cv::Rect selectedBox;
    float selectedConf;
    int selectedClassId;
    
    // Use strategy to select detection
    if (strategy->selectDetection(detections, selectedBox, selectedConf, selectedClassId)) {
        controlUnit.setDetection(selectedBox, frame, frameSeq, selectedClassId);
    } else {
        controlUnit.clearDetection();
    }
}

// Enhanced function to visualize detections with helmet-specific coloring
void visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, const std::vector<std::string>& classNames) {
    if (frame.empty() || detections.empty()) return;

    cv::Mat displayFrame = frame.clone();

    for (const auto& det : detections) {
        if (det.confidence > 0.15f) {
            // Get class name
            std::string className = (det.classId >= 0 && det.classId < static_cast<int>(classNames.size())) ?
                                   classNames[det.classId] : "Unknown";
            
            // Determine box color based on class (helmet safety)
            cv::Scalar boxColor;
            std::string statusText = "";
            
            if (className == "helmet" || className == "hardhat") {
                boxColor = cv::Scalar(0, 255, 0); // Green for safe
                statusText = " ✓ SAFE";
            } else if (className == "no-helmet" || className == "head") {
                boxColor = cv::Scalar(0, 0, 255); // Red for unsafe
                statusText = " ⚠ VIOLATION";
            } else if (className == "person") {
                boxColor = cv::Scalar(255, 165, 0); // Orange for person
                statusText = " - Person";
            } else {
                boxColor = cv::Scalar(0, 255, 255); // Yellow for other classes
            }
            
            // Draw box with appropriate color
            cv::rectangle(displayFrame, det.box, boxColor, 3);

            // Create label with class name, confidence, and status
            std::string label = className + ": " + std::to_string(int(det.confidence * 100)) + "%" + statusText;

            // Add text with background
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
            cv::rectangle(displayFrame,
                         cv::Point(det.box.x, det.box.y - textSize.height - 10),
                         cv::Point(det.box.x + textSize.width, det.box.y),
                         boxColor, -1);

            cv::putText(displayFrame, label,
                       cv::Point(det.box.x, det.box.y - 5),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        }
    }

    // Make sure window is created before showing image
    static bool windowCreated = false;
    if (!windowCreated) {
        cv::namedWindow("Detection View", cv::WINDOW_NORMAL);
        cv::resizeWindow("Detection View", 800, 600);
        windowCreated = true;
    }

    cv::imshow("Detection View", displayFrame);
    cv::waitKey(1);
}

// Updated YOLO detection thread with timing measurement using ModelManager
void threadYolo(ModelManager &modelManager, std::atomic<bool> &running, ControlUnit& controlUnit, int selectionStrategy) {
    while (running) {
        // Wait for our turn to run detection
        if (!controlUnit.waitForDetectionTurn()) {
            continue;
        }

        // Get the latest frame
        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }

        cv::Mat frame = frameData.image;
        if (frame.empty()) continue;

        // Run detection
        std::vector<model::Detection> detections = modelManager.detect(frame);

        // Get class names from model manager
        const std::vector<std::string>& classNames = modelManager.getClassNames();

        // Visualize detections
        visualizeDetections(frame, detections, classNames);

        processDetections(detections, frame, frameData.sequence, controlUnit, selectionStrategy);
    }
}

// Enhanced visualization function with path tracing
void visualizeTracking(cv::Mat& frame, bool isTracking, bool trackerValid, const cv::Rect& lastTrackBox) {
    if (frame.empty()) return;

    cv::Mat displayFrame = frame.clone(); // Create a copy for display
    
    // Static variables to store tracking path
    static std::vector<cv::Point> trackingPath;
    static const int MAX_PATH_POINTS = 50; // Maximum number of path points to store
    static cv::Scalar pathColor = cv::Scalar(255, 100, 0); // Orange color for path
    
    if (isTracking && trackerValid) {
        // Calculate center point of current tracking box
        cv::Point currentCenter(lastTrackBox.x + lastTrackBox.width / 2, 
                               lastTrackBox.y + lastTrackBox.height / 2);
        
        // Add current center to tracking path
        trackingPath.push_back(currentCenter);
        
        // Limit path size to prevent memory growth
        if (trackingPath.size() > MAX_PATH_POINTS) {
            trackingPath.erase(trackingPath.begin());
        }
        
        // Draw tracking path with gradually fading lines
        if (trackingPath.size() > 1) {
            for (size_t i = 1; i < trackingPath.size(); ++i) {
                // Calculate alpha/thickness based on position in path (newer = thicker/brighter)
                float alpha = static_cast<float>(i) / trackingPath.size();
                int thickness = static_cast<int>(1 + alpha * 3); // 1-4 pixel thickness
                
                // Create fading color effect
                cv::Scalar fadeColor = pathColor * alpha;
                
                // Draw line segment
                cv::line(displayFrame, trackingPath[i-1], trackingPath[i], fadeColor, thickness);
            }
            
            // Draw path points as small circles
            for (size_t i = 0; i < trackingPath.size(); ++i) {
                float alpha = static_cast<float>(i) / trackingPath.size();
                int radius = static_cast<int>(2 + alpha * 3); // 2-5 pixel radius
                cv::Scalar pointColor = pathColor * alpha;
                cv::circle(displayFrame, trackingPath[i], radius, pointColor, -1);
            }
        }
        
        // Draw current tracking box with thicker lines for better visibility
        cv::rectangle(displayFrame, lastTrackBox, cv::Scalar(0, 0, 255), 3);

        // Add enhanced text with path info
        std::string label = "Tracking (Path: " + std::to_string(trackingPath.size()) + " points)";
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
        cv::rectangle(displayFrame,
                     cv::Point(lastTrackBox.x, lastTrackBox.y - textSize.height - 10),
                     cv::Point(lastTrackBox.x + textSize.width, lastTrackBox.y),
                     cv::Scalar(0, 0, 255), -1);

        cv::putText(displayFrame, label,
                   cv::Point(lastTrackBox.x, lastTrackBox.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
                   
        // Draw current position marker (bright circle at current center)
        cv::circle(displayFrame, currentCenter, 6, cv::Scalar(0, 255, 255), 2); // Yellow circle
        cv::circle(displayFrame, currentCenter, 3, cv::Scalar(255, 255, 255), -1); // White center
        
    } else {
        // Clear path when tracking is lost
        if (!isTracking) {
            trackingPath.clear();
        }
    }

    // Make sure window is created before showing image
    static bool windowCreated = false;
    if (!windowCreated) {
        cv::namedWindow("Tracker View", cv::WINDOW_NORMAL);
        cv::resizeWindow("Tracker View", 800, 600);
        windowCreated = true;
    }
    cv::imshow("Tracker View", displayFrame);
    cv::waitKey(1); // This is necessary for the window to update
}

// Update the TrackerInterface to include model_initializer
class TrackerInterface {
public:
    virtual ~TrackerInterface() = default;
    virtual void init(const cv::Mat& frame, const cv::Rect& initBox) = 0;
    virtual cv::Rect update(const cv::Mat& frame) = 0;
    virtual bool isInitialized() const = 0;
    // Optional method for SiamFCPP-style initialization
    virtual void model_initializer(const cv::Mat& frame, const cv::Rect& bbox) {
        init(frame, bbox);
    }
    // Add method to get last confidence
    virtual float getLastConfidence() const {
        return 0.0f;
    }
};

// Adapter class for VitTracker
class VitTrackerAdapter : public TrackerInterface {
public:
    explicit VitTrackerAdapter(const std::string& modelPath) : tracker_(modelPath) {}

    void init(const cv::Mat& frame, const cv::Rect& initBox) override {
        tracker_.init(frame, initBox);
    }

    cv::Rect update(const cv::Mat& frame) override {
        return tracker_.update(frame);
    }

    bool isInitialized() const override {
        return tracker_.isInitialized();
    }
private:
    VitTracker tracker_;
};

// SiamFCPPAdapter2 class that adapts our tracker to the TrackerInterface
class SiamFCPPAdapter2 : public TrackerInterface {
public:
    explicit SiamFCPPAdapter2(const std::string& featureModelPath, const std::string& trackModelPath) {
        // Initialize the tracker
        tracker_ = std::make_unique<SiamFCPPTracker2>();

        // Load models
        if (!tracker_->loadModel(featureModelPath, trackModelPath)) {
            throw std::runtime_error("Failed to load SiamFCPP tracker models");
        }
        std::cout << "SiamFCPP tracker models loaded successfully" << std::endl;

    }

    void init(const cv::Mat& frame, const cv::Rect& initBox) override {
        std::cout << "SiamFCPPAdapter2::init - bbox: [" << initBox.x << ", " << initBox.y
                 << ", " << initBox.width << ", " << initBox.height << "]" << std::endl;

        if (!tracker_->init(frame, initBox)) {
            throw std::runtime_error("Failed to initialize SiamFCPP tracker");
        }
        initialized_ = true;
    }

    cv::Rect update(const cv::Mat& frame) override {
        if (!initialized_) {
            throw std::runtime_error("Tracker not initialized");
        }

        float confidence = 0.0f;
        cv::Rect result = tracker_->update(frame, confidence);

        // Store confidence for possible later use
        lastConfidence_ = confidence;

        // Check if tracking is still valid based on confidence and box validity
        if (confidence < 0.25f || //
            result.width <= 0 || result.height <= 0 ||
            result.x < 0 || result.y < 0 ||
            result.x + result.width >= frame.cols ||
            result.y + result.height >= frame.rows) {

            std::cout << "Tracking failed - confidence: " << confidence
                      << ", box: " << result.x << "," << result.y << ","
                      << result.width << "," << result.height << std::endl;

            // Introduce a counter to make tracking failure more robust
            failureCount_++;
            
            // Only declare tracking lost after multiple consecutive failures
            if (failureCount_ >= 3) {
                std::cout << "Too many consecutive failures, tracking lost" << std::endl;
                initialized_ = false;
                return cv::Rect(0, 0, 0, 0);  // Return empty rect to indicate failure
            } else {
                // Return the last valid result for a few frames to handle temporary low confidence
                return lastValidResult_;
            }
        }
        
        // Reset failure counter and store valid result
        failureCount_ = 0;
        lastValidResult_ = result;
        
        return result;
    }

    bool isInitialized() const override {
        return initialized_;
    }

    // Override model_initializer to use our init method
    void model_initializer(const cv::Mat& frame, const cv::Rect& bbox) override {
        init(frame, bbox);
    }

    // Implement getLastConfidence to return the stored value
    float getLastConfidence() const override {
        return lastConfidence_;
    }

private:
    std::unique_ptr<SiamFCPPTracker2> tracker_;
    bool initialized_ = false;
    float lastConfidence_ = 0.0f;
    cv::Rect lastValidResult_;
    int failureCount_ = 0;
};

// Updated tracker thread to use the abstract interface
void threadTracker(std::atomic<bool> &running, std::unique_ptr<TrackerInterface>& tracker,
                  ModelManager& modelManager, ControlUnit& controlUnit) {
    bool isTracking = false;
    cv::Rect lastTrackBox;
    int mode = controlUnit.getDetectionMode();

    while (running) {
        // Wait for turn to run tracking
        if (!controlUnit.waitForTrackingTurn()) {
            continue;
        }

        // Check for new detection to initialize
        bool shouldInitialize = controlUnit.hasNewDetection();
        cv::Rect yoloBox;
        cv::Mat detectionFrame;
        int classId = -1;

        if (shouldInitialize) {
            controlUnit.getDetectionData(yoloBox, detectionFrame, classId);
            controlUnit.markDetectionAsProcessed();

            if (!detectionFrame.empty()) {
                // Print the detection box dimensions for debugging
                std::cout << "Detection box: x=" << yoloBox.x << ", y=" << yoloBox.y
                         << ", width=" << yoloBox.width << ", height=" << yoloBox.height << std::endl;
            }
        }

        if ((shouldInitialize && mode == 0) || (mode == 1 && !isTracking)) {
            if (!detectionFrame.empty()) {
                try {
                    // Use model_initializer for all trackers
                    tracker->model_initializer(detectionFrame, yoloBox);

                    isTracking = true;
                    lastTrackBox = yoloBox;
                    
                    // Get class names from model manager
                    const std::vector<std::string>& classNames = modelManager.getClassNames();
                    std::string className = (classId >= 0 && classId < static_cast<int>(classNames.size())) ?
                                          classNames[classId] : "Unknown";
                    std::cout << "Tracker: Initialized, Class: " << className << std::endl;

                    // Visualize initial detection box
                    visualizeTracking(detectionFrame, true, true, yoloBox);
                } catch (const std::exception& e) {
                    std::cerr << "Tracker initialization failed: " << e.what() << std::endl;
                    isTracking = false;
                }
            }
        }

        // Get frame for update
        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) continue;

        // Update tracker if tracking
        bool trackerValid = false;
        if (isTracking) {
            try {
                // Start timing
                auto startTime = std::chrono::high_resolution_clock::now();
                
                lastTrackBox = tracker->update(frame);
                
                // End timing and calculate FPS
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
                float currentFps = 1000000.0f / duration; // Convert microseconds to seconds for FPS
                trackerValid = lastTrackBox.width > 0 && lastTrackBox.height > 0 && tracker->isInitialized();
                if (!trackerValid) {
                    isTracking = false;
                    controlUnit.setTrackerFailed(true);
                    std::cout << "Tracker: Tracking lost, last confidence: " << tracker->getLastConfidence() << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Tracker update failed: " << e.what() << std::endl;
                isTracking = false;
                controlUnit.setTrackerFailed(true);
            }
        }

        // Always visualize, even if not tracking
        visualizeTracking(frame, isTracking, trackerValid, lastTrackBox);
    }
    cv::destroyAllWindows(); // Close all windows properly
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

        // Set up the appropriate tracker based on configuration
        std::unique_ptr<TrackerInterface> tracker;

        if (trackerType == 0) {
            std::cout << "Using VitTracker" << std::endl;
            tracker = std::make_unique<VitTrackerAdapter>(vitTrackerModelPath);
        } else {
            std::cout << "Using SiamFCPP tracker" << std::endl;
            tracker = std::make_unique<SiamFCPPAdapter2>(siamfcFeatureModelPath,
                                                         siamfcTrackingModelPath);
        }

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
