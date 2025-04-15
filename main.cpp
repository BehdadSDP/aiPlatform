#include "camera_handler.h"
#include "frame_buffer_manager.h"
#include "model.h"
#include "shared_data.h"
#include "vittracker.h"
#include "control_unit.h"
#include "config_utils.h"
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

// Updated to use ControlUnit
void processDetections(const std::vector<model::Detection>& detections, const cv::Mat& frame, uint64_t frameSeq, ControlUnit& controlUnit) {
    float bestConf = -1.0f;
    cv::Rect bestBox;
    int bestClassId = -1;
    
    for (const auto &det : detections) {
        if (det.confidence > bestConf) {
            bestConf = det.confidence;
            bestBox = det.box;
            bestClassId = det.classId;
        }
    }

    if (bestConf > 0.25f) {
        controlUnit.setDetection(bestBox, frame, frameSeq, bestClassId);
    } else {
        controlUnit.clearDetection();
    }
}

// Add this new function to visualize detections
void visualizeDetections(cv::Mat& frame, const std::vector<model::Detection>& detections, const std::vector<std::string>& classNames) {
    if (frame.empty() || detections.empty()) return;

    cv::Mat displayFrame = frame.clone();

    for (const auto& det : detections) {
        if (det.confidence > 0.25f) {
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


// Updated YOLO detection thread
void threadYolo(model &yoloDetector, std::atomic<bool> &running, ControlUnit& controlUnit, const std::vector<std::string>& classNames) {
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
        std::vector<model::Detection> detections = yoloDetector.detect(frame);
        
        // Visualize detections
        visualizeDetections(frame, detections, classNames);
        
        if (controlUnit.getDetectionMode() == 1) {
            std::cout << "YOLO: Detected " << detections.size() << " objects" << std::endl;
        }
        
        processDetections(detections, frame, frameData.sequence, controlUnit);
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

// Updated tracker thread
void threadTracker(std::atomic<bool> &running, VitTracker& tracker, 
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
                tracker.init(detectionFrame, yoloBox);
                isTracking = true;
                lastTrackBox = yoloBox;
                std::string className = (classId >= 0 && classId < static_cast<int>(classNames.size())) ? 
                                       classNames[classId] : "Unknown";
                std::cout << "Tracker: Initialized, Class: " << className << std::endl;
                
                // Visualize initial detection box
                visualizeTracking(detectionFrame, true, true, yoloBox);
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
            lastTrackBox = tracker.update(frame);
            trackerValid = lastTrackBox.width > 0 && lastTrackBox.height > 0 && tracker.isInitialized();
            
            // Print tracking box for debugging
            if (trackerValid) {
                std::cout << "Tracking box: x=" << lastTrackBox.x << ", y=" << lastTrackBox.y 
                         << ", width=" << lastTrackBox.width << ", height=" << lastTrackBox.height << std::endl;
            }
            
            if (!trackerValid) {
                isTracking = false;
                controlUnit.setTrackerFailed(true);
                std::cout << "Tracker: Tracking lost" << std::endl;
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
        auto config = config_utils::loadConfig("/home/pi5/shared_folder/aiPlatform/config.txt");
        int resolutionIndex = config_utils::getConfigInt(config, "camera.resolution_index");
        int customWidth = config_utils::getConfigInt(config, "camera.width");
        int customHeight = config_utils::getConfigInt(config, "camera.height");
        float frameRate = config_utils::getConfigFloat(config, "camera.frame_rate");
        int detectionInterval = config_utils::getConfigInt(config, "detection.interval");
        int trackingInterval = config_utils::getConfigInt(config, "tracking.interval");
        int targetClassId = config_utils::getConfigInt(config, "general.target_class_id");
        int detectionMode = config_utils::getConfigInt(config, "detection.mode");
        int trackingMode = config_utils::getConfigInt(config, "tracking.mode");

        model yoloDetector("/home/pi5/ai_platform/aiPlatform/models/yolov12m.onnx",
                           "/home/pi5/ai_platform/aiPlatform/models/coco.names",
                           targetClassId);

        // Initialize ViT model and class names
        std::unique_ptr<VitTracker> vitTracker = std::make_unique<VitTracker>("/home/pi5/ai_platform/aiPlatform/models/object_tracking_vittrack_2023sep.onnx");
        std::vector<std::string> classNames;
        std::ifstream classFile("/home/pi5/ai_platform/aiPlatform/models/coco.names");
        if (classFile.is_open()) {
            std::string line;
            while (std::getline(classFile, line)) {
                line.erase(0, line.find_first_not_of(" \t"));
                line.erase(line.find_last_not_of(" \t") + 1);
                if (!line.empty()) classNames.push_back(line);
            }
            classFile.close();
        }

        std::atomic<bool> running(true);
        g_running = &running;
        signal(SIGINT, signalHandler);

        cameraHandler.initialize();
        cameraHandler.acquireCamera();
        cameraHandler.configureCamera(resolutionIndex, customWidth, customHeight);
        cameraHandler.setFrameRate(frameRate);

        controlUnit.setDetectionMode(detectionMode);
        controlUnit.setDetectionInterval(detectionInterval);
        controlUnit.setTrackingInterval(trackingInterval);

        cameraHandler.startStreaming();

        std::thread yoloThread(threadYolo, std::ref(yoloDetector), std::ref(running), 
                              std::ref(controlUnit), std::ref(classNames));
        std::thread trackerThread(threadTracker, std::ref(running), std::ref(*vitTracker), 
                                 std::ref(classNames), std::ref(controlUnit));
        std::cout << "Streaming... Press Ctrl+C to exit." << std::endl;

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        cameraHandler.stopStreaming();
        running = false;
        yoloThread.join();
        trackerThread.join();

        std::cout << "Camera streaming stopped." << std::endl;
    }
    catch (const CameraException &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception &e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}
