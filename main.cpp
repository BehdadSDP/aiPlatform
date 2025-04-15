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

// Utility function to process detections
void processDetections(const std::vector<model::Detection>& detections, const cv::Mat& frame, uint64_t frameSeq, SingleObjectData& sharedData) {
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

    std::lock_guard<std::mutex> lock(sharedData.mtx);
    if (bestConf > 0.25f) {
        sharedData.detection.box = bestBox;
        sharedData.detection.frame = frame.clone();
        sharedData.detection.frameSeq = frameSeq;
        sharedData.detection.classId = bestClassId;
        sharedData.detection.valid = true; // Always set for tracker init
        sharedData.detection.newDetection = true;
        sharedData.trackerFailed = false; // Clear on new detection
    } else {
        sharedData.detection.valid = false;
        sharedData.detection.newDetection = false;
        sharedData.detection.frame.release();
        sharedData.detection.classId = -1;
    }
}

// Updated threadYolo
void threadYolo(model &yoloDetector, SingleObjectData &sharedData, std::atomic<bool> &running, ControlUnit& controlUnit, int detectionMode) {
    bool alreadyDetected = false;
    bool trackerFailed = false;
    while (running) {
        if (detectionMode == 0 && controlUnit.shouldDetect()) {
            FrameData frameData;
            if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
//                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            cv::Mat frame = frameData.image;
            if (frame.empty()) continue;
            std::vector<model::Detection> detections = yoloDetector.detect(frame);
            processDetections(detections, frame, frameData.sequence, sharedData);
        }
        else if (detectionMode == 1) {
            {
                std::lock_guard<std::mutex> lock(sharedData.mtx);
                alreadyDetected = sharedData.detection.valid;
                trackerFailed = sharedData.trackerFailed;
            }
            if (alreadyDetected && !trackerFailed) {
//                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            FrameData frameData;
            if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
//                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if(!alreadyDetected || (trackerFailed)){
                cv::Mat frame = frameData.image;
                if (frame.empty()) continue;
                std::vector<model::Detection> detections = yoloDetector.detect(frame);
                std::cout << "YOLO: Detected " << detections.size() << " objects" << std::endl;
                processDetections(detections, frame, frameData.sequence, sharedData);
            }

        }
    }
}

// Visualization function
void visualizeTracking(const cv::Mat& frame, bool isTracking, bool trackerValid, const cv::Rect& lastTrackBox) {
    if (!frame.empty()) {
        if (isTracking && trackerValid) {
            cv::rectangle(frame, lastTrackBox, cv::Scalar(0, 0, 255), 2);
            cv::putText(frame, "Tracking", lastTrackBox.tl(),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
        }
        cv::imshow("Tracker Thread View", frame);
        cv::waitKey(1);
    }
}

// Updated threadTracker
void threadTracker(SingleObjectData &sharedData, std::atomic<bool> &running, VitTracker& tracker, const std::vector<std::string>& classNames, ControlUnit& controlUnit, int detectionMode) {
    bool isTracking = false;
    cv::Rect lastTrackBox;
    while (running) {
        // Check for new detection to initialize
        bool shouldInitialize = false;
        cv::Rect yoloBox;
        cv::Mat detectionFrame;
        int classId;
        {
            std::lock_guard<std::mutex> lock(sharedData.mtx);
            if (sharedData.detection.newDetection && sharedData.detection.valid) {
                shouldInitialize = true;
                yoloBox = sharedData.detection.box;
                detectionFrame = sharedData.detection.frame.clone();
                classId = sharedData.detection.classId;
                sharedData.detection.newDetection = false;
//                sharedData.detection.valid = false; // Clear for next detection
//                sharedData.trackerFailed = false; // Reset on initialization
            }
        }

        if ((shouldInitialize && detectionMode == 0) || (detectionMode == 1 && isTracking == false)) {
            if (detectionFrame.empty()) {
                std::cout << "Tracker: Empty detection frame, skipping init" << std::endl;
                continue;
            }
            tracker.init(detectionFrame, yoloBox);
            std::cout << "shoudlDetect_frame_counter:" << controlUnit.getDetectionFrameCounter() << std::endl;
            isTracking = true;
            lastTrackBox = yoloBox;
            std::string className = (classId >= 0 && classId < static_cast<int>(classNames.size())) ? classNames[classId] : "Unknown";
            std::cout << "Tracker: Initialized, Class: " << className << std::endl;
            shouldInitialize = false;
        }

        // Get frame for update
        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
//            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) continue;

        // Update tracker if tracking
        bool trackerValid = false;
        if (isTracking) {
            lastTrackBox = tracker.update(frame);
            trackerValid = lastTrackBox.width > 0 && lastTrackBox.height > 0 && tracker.isInitialized();
            if (!trackerValid) {
                isTracking = false;
                {
                    std::lock_guard<std::mutex> lock(sharedData.mtx);
                    sharedData.trackerFailed = true; // Signal YOLO to detect
                }
                std::cout << "Tracker: Tracking lost" << std::endl;
            }
        }
        // Visualize
        visualizeTracking(frame, isTracking, trackerValid, lastTrackBox);
    }
    cv::destroyWindow("Tracker Thread View");
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

        controlUnit.setDetectionFrameInterval(detectionInterval);
//        controlUnit.setTrackingFrameInterval(trackingInterval);

        SingleObjectData singleObjData;

        cameraHandler.startStreaming();

        std::thread yoloThread(threadYolo, std::ref(yoloDetector), std::ref(singleObjData), std::ref(running), std::ref(controlUnit), detectionMode);
        std::thread trackerThread(threadTracker, std::ref(singleObjData), std::ref(running), std::ref(*vitTracker), std::ref(classNames), std::ref(controlUnit), detectionMode);
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
