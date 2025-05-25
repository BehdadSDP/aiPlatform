#include "include/camera_handler.h"
#include "include/frame_buffer_manager.h"
#include "include/model.h"
#include "include/vittracker.h"
#include "include/siamfc_pp_tracker.h"
#include "include/control_unit.h"
#include "include/config_utils.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <csignal>
#include <memory>
#include <fstream>
#include <vector>

std::atomic<bool>* g_running = nullptr;

void signalHandler(int) {
    if (g_running) g_running->store(false);
}

// Updated to use ControlUnit and support multiple selection strategies
void processDetections(const std::vector<model::Detection>& detections, const cv::Mat& frame, uint64_t frameSeq, ControlUnit& controlUnit, int selectionStrategy) {
    if (detections.empty()) {
        controlUnit.clearDetection();
        return;
    }

    cv::Rect selectedBox;
    float selectedConf = -1.0f;
    int selectedClassId = -1;
    
    // Strategy 0: Highest confidence (default)
    if (selectionStrategy == 0) {
        for (const auto &det : detections) {
            if (det.confidence > selectedConf) {
                selectedConf = det.confidence;
                selectedBox = det.box;
                selectedClassId = det.classId;
            }
        }
    }
    // Strategy 1: Upper bounding box (smallest y coordinate)
    else if (selectionStrategy == 1) {
        int minY = INT_MAX;
        for (const auto &det : detections) {
            if (det.confidence > 0.15f && det.box.y < minY) {
                minY = det.box.y;
                selectedBox = det.box;
                selectedConf = det.confidence;
                selectedClassId = det.classId;
            }
        }
    }
    // Strategy 2: Lower bounding box (largest y coordinate)
    else if (selectionStrategy == 2) {
        int maxY = -1;
        for (const auto &det : detections) {
            if (det.confidence > 0.15f && det.box.y > maxY) {
                maxY = det.box.y;
                selectedBox = det.box;
                selectedConf = det.confidence;
                selectedClassId = det.classId;
            }
        }
    }
    // Strategy 3: Rightmost bounding box (largest x coordinate)
    else if (selectionStrategy == 3) {
        int maxX = -1;
        for (const auto &det : detections) {
            if (det.confidence > 0.15f && det.box.x + det.box.width > maxX) {
                maxX = det.box.x + det.box.width;
                selectedBox = det.box;
                selectedConf = det.confidence;
                selectedClassId = det.classId;
            }
        }
    }
    // Strategy 4: Leftmost bounding box (smallest x coordinate)
    else if (selectionStrategy == 4) {
        int minX = INT_MAX;
        for (const auto &det : detections) {
            if (det.confidence > 0.15f && det.box.x < minX) {
                minX = det.box.x;
                selectedBox = det.box;
                selectedConf = det.confidence;
                selectedClassId = det.classId;
            }
        }
    }
    
    // Only set the detection if we found a valid one with confidence > 0.15
    if (selectedConf > 0.15f) {
        controlUnit.setDetection(selectedBox, frame, frameSeq, selectedClassId);
    } else {
        controlUnit.clearDetection();
    }
}

// Add this new function to visualize detections
void visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, const std::vector<std::string>& classNames) {
    if (frame.empty() || detections.empty()) return;

    cv::Mat displayFrame = frame.clone();

    for (const auto& det : detections) {
        if (det.confidence > 0.15f) {
            // Draw box
            cv::rectangle(displayFrame, det.box, cv::Scalar(0, 255, 0), 2);

            // Create label with class name and confidence
            std::string className = (det.classId >= 0 && det.classId < static_cast<int>(classNames.size())) ?
                                   classNames[det.classId] : "Unknown";
            std::string label = className + ": " + std::to_string(int(det.confidence * 100)) + "%";

            // Add text with background
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
            cv::rectangle(displayFrame,
                         cv::Point(det.box.x, det.box.y - textSize.height - 5),
                         cv::Point(det.box.x + textSize.width, det.box.y),
                         cv::Scalar(0, 255, 0), -1);

            cv::putText(displayFrame, label,
                       cv::Point(det.box.x, det.box.y - 5),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
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

// Updated YOLO detection thread with timing measurement
void threadYolo(model &yoloDetector, std::atomic<bool> &running, ControlUnit& controlUnit, const std::vector<std::string>& classNames, int selectionStrategy) {
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

        // Measure detection time
        auto start = std::chrono::high_resolution_clock::now();

        // Run detection
        std::vector<model::Detection> detections = yoloDetector.detect(frame);

        // Calculate and print detection time
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "YOLO detection time: " << duration << "ms" << std::endl;

        // Visualize detections
        visualizeDetections(frame, detections, classNames);

        if (controlUnit.getDetectionMode() == 1) {
            std::cout << "YOLO: Detected " << detections.size() << " objects" << std::endl;
        }
        processDetections(detections, frame, frameData.sequence, controlUnit, selectionStrategy);
    }
}

// Updated visualization function
void visualizeTracking(cv::Mat& frame, bool isTracking, bool trackerValid, const cv::Rect& lastTrackBox) {
    if (frame.empty()) return;

    cv::Mat displayFrame = frame.clone(); // Create a copy for display

    if (isTracking && trackerValid) {
        // Draw box with thicker lines for better visibility
        cv::rectangle(displayFrame, lastTrackBox, cv::Scalar(0, 0, 255), 2);

        // Add text with background for better visibility
        std::string label = "Tracking";
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, &baseline);
        cv::rectangle(displayFrame,
                     cv::Point(lastTrackBox.x, lastTrackBox.y - textSize.height - 5),
                     cv::Point(lastTrackBox.x + textSize.width, lastTrackBox.y),
                     cv::Scalar(0, 0, 255), -1);

        cv::putText(displayFrame, label,
                   cv::Point(lastTrackBox.x, lastTrackBox.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
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
        if (confidence < 0.1f || //
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
                  const std::vector<std::string>& classNames, ControlUnit& controlUnit) {
    bool isTracking = false;
    cv::Rect lastTrackBox;
    int mode = controlUnit.getDetectionMode();

    while (running) {
        // Wait for our turn to run tracking
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
                lastTrackBox = tracker->update(frame);
                trackerValid = lastTrackBox.width > 0 && lastTrackBox.height > 0 && tracker->isInitialized();

                // Print tracking box and confidence for debugging
                if (trackerValid) {
                    std::cout << "Tracking box: x=" << lastTrackBox.x << ", y=" << lastTrackBox.y
                             << ", width=" << lastTrackBox.width << ", height=" << lastTrackBox.height
                             << ", confidence=" << tracker->getLastConfidence() << std::endl;
                }

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
        ControlUnit controlUnit;
        CameraHandler cameraHandler(controlUnit);

        // Config related code
        auto config = config_utils::loadConfig("/home/pi5/shared_folder/aiPlatform/config/config.txt");
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

        // Read file paths from config
        std::string yoloModelPath = config_utils::getConfigString(config, "general.yolo_model_path");
        std::string cocoNamesPath = config_utils::getConfigString(config, "general.coco_names_path");
        std::string vitTrackerModelPath = config_utils::getConfigString(config, "general.vittracker_model_path");
        std::string siamfcFeatureModelPath = config_utils::getConfigString(config, "general.siamfc_feature_model_path");
        std::string siamfcTrackingModelPath = config_utils::getConfigString(config, "general.siamfc_tracking_model_path");

        // Initialize CameraHandler
        cameraHandler.initialize();
        cameraHandler.acquireCamera();
        cameraHandler.configureCamera(resolutionIndex, customWidth, customHeight);
        cameraHandler.setFrameRate(frameRate);

        // Configure ControlUnit
        controlUnit.setDetectionMode(detectionMode);
        controlUnit.setDetectionInterval(detectionInterval);
        controlUnit.setTrackingInterval(trackingInterval);

        // Start camera streaming
        cameraHandler.startStreaming();

        // Create model
        model yoloDetector(yoloModelPath,
                           cocoNamesPath,
                           targetClassId);

        // Load class names for visualization purposes
        std::vector<std::string> classNames;
        std::ifstream classFile(cocoNamesPath);
        std::string className;
        while (std::getline(classFile, className)) {
            classNames.push_back(className);
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
        // Set up atomic flag for signal handling
        std::atomic<bool> running(true);
        g_running = &running;
        std::signal(SIGINT, signalHandler);

        // Print starting message
        std::cout << "Streaming... Press Ctrl+C to exit." << std::endl;

        // Start worker threads
        std::thread yoloThread(threadYolo, std::ref(yoloDetector), std::ref(running),
                              std::ref(controlUnit), std::ref(classNames), selectionStrategy);

        std::thread trackerThread(threadTracker, std::ref(running), std::ref(tracker),
                                 std::ref(classNames), std::ref(controlUnit));

        // Join threads when done
        yoloThread.join();
        trackerThread.join();

        // Clean up
        cameraHandler.cleanup();
        std::cout << "Cleanup complete." << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}