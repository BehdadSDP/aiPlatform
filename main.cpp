#include "camera_handler.h"
#include "frame_buffer_manager.h"
#include "model.h"
#include "shared_data.h"
#include "vittracker.h"
#include "control_unit.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <csignal>
#include <fstream>
#include <sstream>
#include <map>

static constexpr float CONF_THRESHOLD = 0.5f;

std::atomic<bool>* g_running = nullptr;
void signalHandler(int) { if (g_running) g_running->store(false); }

// Function to trim whitespace from strings
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, last - first + 1);
}

// Function to load configuration from file
std::map<std::string, std::string> loadConfig(const std::string& filename) {
    std::map<std::string, std::string> config;
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filename);
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        size_t delimiterPos = line.find('=');
        if (delimiterPos == std::string::npos) continue;

        std::string key = trim(line.substr(0, delimiterPos));
        std::string value = trim(line.substr(delimiterPos + 1));
        config[key] = value;
    }
    file.close();
    return config;
}

void threadYolo(model &yoloDetector, SingleObjectData &sharedData, std::atomic<bool> &running, ControlUnit& controlUnit) {
    while (running) {
        std::string detectorState = (controlUnit.getDetectionMode() == ControlUnit::Mode::RUN) ? "RUN" : "STANDBY";
        if (controlUnit.getDetectionMode() == ControlUnit::Mode::RUN && controlUnit.shouldDetect()) {
            FrameData frameData;
            if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            cv::Mat frame = frameData.image;
            if (frame.empty()) continue;

            std::vector<model::Detection> detections = yoloDetector.detect(frame);

            float bestConf = -1.0f;
            cv::Rect bestBox;
            int bestClassId = -1;
            for (const auto &det : detections) {
                if (det.confidence > bestConf) {
                    bestConf = det.confidence;
                    bestBox = det.box;
                    bestClassId = det.classId; // Store classId
                }
            }

            {
                std::lock_guard<std::mutex> lock(sharedData.mtx);
                if (bestConf > 0.25f) {
                    sharedData.detection.box = bestBox;
                    sharedData.detection.valid = true;
                    sharedData.detection.frame = frame.clone();
                    sharedData.detection.frameSeq = frameData.sequence;
                    sharedData.detection.newDetection = true;
                    sharedData.detection.classId = bestClassId; // Add classId to sharedData
                    std::cout << "Detector: " << detectorState << std::endl;
                } else {
                    sharedData.detection.valid = false;
                    sharedData.detection.newDetection = false;
                    sharedData.detection.frame.release();
                    sharedData.detection.classId = -1;
                }
            }
            sharedData.cv.notify_one();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        controlUnit.incrementDetectionFrameCounter();
    }
}

void threadTracker(SingleObjectData &sharedData, std::atomic<bool> &running, ControlUnit& controlUnit) {
    const std::string vitModelPath = "/home/pi5/ai_platform/aiPlatform/models/object_tracking_vittrack_2023sep.onnx";
    std::unique_ptr<VitTracker> tracker;
    bool isTracking = false;
    cv::Rect lastTrackBox;

    // Class names for logging (load from coco.names or similar)
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

    while (running) {
        std::string trackerState = (controlUnit.getTrackingMode() == ControlUnit::Mode::RUN) ? "RUN" : "STANDBY";

        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) continue;

        bool trackerValid = false;
        if (tracker && isTracking) {
            lastTrackBox = tracker->update(frame);
            trackerValid = lastTrackBox.width > 0 && lastTrackBox.height > 0 && tracker->isInitialized();
        }

        ControlUnit::Action action = controlUnit.decideAction(sharedData, isTracking, lastTrackBox);

        switch (action) {
        case ControlUnit::Action::INITIALIZE: {
            cv::Rect yoloBox;
            cv::Mat detectionFrame;
            int classId;
            {
                std::lock_guard<std::mutex> lock(sharedData.mtx);
                yoloBox = sharedData.detection.box;
                detectionFrame = sharedData.detection.frame.clone();
                classId = sharedData.detection.classId;
                sharedData.detection.newDetection = false;
            }
            tracker = std::make_unique<VitTracker>(vitModelPath);
            tracker->init(detectionFrame, yoloBox);
            isTracking = true;
            lastTrackBox = yoloBox;
            std::string className = (classId >= 0 && classId < classNames.size()) ? classNames[classId] : "Unknown";
            std::cout << "Tracker: " << trackerState << ", Class: " << className << std::endl;
            break;
        }
        case ControlUnit::Action::CONTINUE:
            if (!trackerValid && isTracking) {
                isTracking = false;
                tracker.reset();
            }
            break;
        case ControlUnit::Action::STOP:
            isTracking = false;
            tracker.reset();
            std::cout << "Tracker: " << trackerState << std::endl;
            break;
        case ControlUnit::Action::REINITIALIZE:
            break;
        }

        if (!frame.empty()) {
            if (isTracking && trackerValid) {
                cv::rectangle(frame, lastTrackBox, cv::Scalar(0, 0, 255), 2);
                cv::putText(frame, "Tracking", lastTrackBox.tl(),
                            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
            }
            cv::imshow("Tracker Thread View", frame);
            cv::waitKey(1);
        }

        controlUnit.incrementTrackingFrameCounter();
    }

    cv::destroyWindow("Tracker Thread View");
}

int main() {
    try {
        ControlUnit controlUnit;
        CameraHandler cameraHandler(controlUnit);

        auto config = loadConfig("/home/pi5/ai_platform/aiPlatform/config.txt");

        int resolutionIndex = std::stoi(config["camera_resolution_index"]);
        int customWidth = config.find("custom_width") != config.end() ? std::stoi(config["custom_width"]) : 0;
        int customHeight = config.find("custom_height") != config.end() ? std::stoi(config["custom_height"]) : 0;
        float frameRate = std::stof(config["frame_rate"]);
        int detectionInterval = std::stoi(config["detection_interval"]);
        int trackingInterval = 1; // Set to 1 for faster response
        int targetClassId = std::stoi(config["target_class_id"]);

        model yoloDetector("/home/pi5/ai_platform/aiPlatform/models/yolov12m.onnx",
                           "/home/pi5/ai_platform/aiPlatform/models/coco.names",
                           targetClassId);

        std::atomic<bool> running(true);
        g_running = &running;
        signal(SIGINT, signalHandler);

        cameraHandler.initialize();
        cameraHandler.acquireCamera();
        cameraHandler.configureCamera(resolutionIndex, customWidth, customHeight);
        cameraHandler.setFrameRate(frameRate);

        // Explicitly set correct initial modes
        controlUnit.setDetectionMode(ControlUnit::Mode::RUN);    // YOLO starts in RUN
        controlUnit.setTrackingMode(ControlUnit::Mode::STANDBY); // Tracker starts in STANDBY
        controlUnit.setDetectionFrameInterval(detectionInterval);
        controlUnit.setTrackingFrameInterval(trackingInterval);

        SingleObjectData singleObjData;
        {
            std::lock_guard<std::mutex> lock(singleObjData.mtx);
            singleObjData.detection.valid = false;
            singleObjData.detection.frameSeq = 0;
        }

        cameraHandler.startStreaming();

        std::thread yoloThread(threadYolo, std::ref(yoloDetector), std::ref(singleObjData), std::ref(running), std::ref(controlUnit));
        std::thread trackerThread(threadTracker, std::ref(singleObjData), std::ref(running), std::ref(controlUnit));

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
