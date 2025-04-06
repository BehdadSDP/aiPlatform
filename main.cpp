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
        if (controlUnit.getDetectionMode() == ControlUnit::Mode::RUN && controlUnit.shouldDetect()) {
            FrameBufferManager::getInstance().waitForNewFrame();

            FrameData frameData;
            if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            cv::Mat frame = frameData.image;
            if (frame.empty()) {
                continue;
            }

            std::vector<model::Detection> detections = yoloDetector.detect(frame);

            float bestConf = -1.0f;
            cv::Rect bestBox; // Fixed: Removed invalid '>' character
            for (const auto &det : detections) {
                if (det.confidence > bestConf) {
                    bestConf = det.confidence;
                    bestBox = det.box;
                }
            }

            {
                std::lock_guard<std::mutex> lock(sharedData.mtx);
                if (bestConf > CONF_THRESHOLD) {
                    sharedData.detection.box = bestBox;
                    sharedData.detection.valid = true;
                    sharedData.detection.frame = frame.clone();
                    sharedData.detection.frameSeq = frameData.sequence;
                    sharedData.detection.newDetection = true;
                    std::cout << "YOLO detection: Box = " << bestBox << ", Confidence = " << bestConf << std::endl;
                } else {
                    sharedData.detection.valid = false;
                    sharedData.detection.newDetection = false;
                    sharedData.detection.frame.release();
                    std::cout << "YOLO detection: No valid detection (best confidence = " << bestConf << ")" << std::endl;
                }
            }
            sharedData.cv.notify_one();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void threadTracker(SingleObjectData &sharedData, std::atomic<bool> &running, ControlUnit& controlUnit) {
    const std::string vitModelPath = "/home/pi5/ai_platform/aiPlatform/models/object_tracking_vittrack_2023sep.onnx";
    std::unique_ptr<VitTracker> tracker;
    bool isTracking = false;
    int framesWithoutDetection = 0;
    const int maxFramesWithoutDetection = 30;
    cv::Rect lastTrackBox;

    while (running) {
        if (controlUnit.getTrackingMode() == ControlUnit::Mode::RUN || isTracking) {
            FrameBufferManager::getInstance().waitForNewFrame();

            FrameData frameData;
            if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            cv::Mat frame = frameData.image;
            if (frame.empty()) {
                continue;
            }

            if (controlUnit.getTrackingMode() == ControlUnit::Mode::RUN) {
                bool trackerValid = false;

                if (tracker && isTracking) {
                    lastTrackBox = tracker->update(frame);
                    trackerValid = lastTrackBox.width > 0 && lastTrackBox.height > 0 && tracker->isInitialized();
                    std::cout << "Tracker updated: " << lastTrackBox << ", Valid: " << trackerValid << std::endl;
                }

                if (controlUnit.shouldTrack()) {
                    ControlUnit::Action action = controlUnit.decideAction(sharedData, isTracking, lastTrackBox);

                    switch (action) {
                    case ControlUnit::Action::INITIALIZE: {
                        cv::Rect yoloBox;
                        {
                            std::lock_guard<std::mutex> lock(sharedData.mtx);
                            yoloBox = sharedData.detection.box;
                        }
                        tracker = std::make_unique<VitTracker>(vitModelPath);
                        tracker->init(sharedData.detection.frame, yoloBox);
                        isTracking = true;
                        framesWithoutDetection = 0;
                        std::cout << "Tracker initialized with YOLO box: " << yoloBox << std::endl;
                        break;
                    }
                    case ControlUnit::Action::REINITIALIZE: {
                        cv::Rect yoloBox;
                        {
                            std::lock_guard<std::mutex> lock(sharedData.mtx);
                            yoloBox = sharedData.detection.box;
                        }
                        tracker->init(sharedData.detection.frame, yoloBox);
                        isTracking = true;
                        framesWithoutDetection = 0;
                        std::cout << "Tracker reinitialized with YOLO box: " << yoloBox << std::endl;
                        break;
                    }
                    case ControlUnit::Action::CONTINUE:
                        if (!trackerValid && isTracking) {
                            isTracking = false;
                            tracker.reset();
                            std::cout << "Tracker stopped: Invalid tracking result" << std::endl;
                        }
                        break;
                    case ControlUnit::Action::STOP:
                        isTracking = false;
                        tracker.reset();
                        std::cout << "Tracker stopped by control unit" << std::endl;
                        break;
                    }
                }

                bool yoloValid = false;
                {
                    std::lock_guard<std::mutex> lock(sharedData.mtx);
                    yoloValid = sharedData.detection.valid;
                }
                if (isTracking && !yoloValid) {
                    framesWithoutDetection++;
                    if (framesWithoutDetection >= maxFramesWithoutDetection) {
                        isTracking = false;
                        tracker.reset();
                        std::cout << "Tracker stopped: No detection for " << maxFramesWithoutDetection << " frames" << std::endl;
                    }
                } else if (yoloValid) {
                    framesWithoutDetection = 0;
                }

                if (isTracking && trackerValid) {
                    cv::rectangle(frame, lastTrackBox, cv::Scalar(0, 0, 255), 2);
                    cv::putText(frame, "Tracking", lastTrackBox.tl(),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
                }
            } else {
                isTracking = false;
                tracker.reset();
                framesWithoutDetection = 0;
            }

            cv::imshow("Tracker Thread View", frame);
            cv::waitKey(1);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    cv::destroyWindow("Tracker Thread View");
}

int main() {
    try {
        ControlUnit controlUnit;
        CameraHandler cameraHandler(controlUnit);
        model yoloDetector("/home/pi5/ai_platform/aiPlatform/models/yolov4-tiny.cfg",
                           "/home/pi5/ai_platform/aiPlatform/models/yolov4-tiny.weights",
                           "/home/pi5/ai_platform/aiPlatform/models/coco.names");

        std::atomic<bool> running(true);
        g_running = &running;
        signal(SIGINT, signalHandler);

        cameraHandler.initialize();
        cameraHandler.acquireCamera();

        // Load configuration
        auto config = loadConfig("/home/pi5/ai_platform/aiPlatform/config.txt");

        // Check for required config keys and set defaults if missing
        if (config.find("camera_resolution_index") == config.end()) throw std::runtime_error("Missing 'camera_resolution_index' in config");
        if (config.find("frame_rate") == config.end()) throw std::runtime_error("Missing 'frame_rate' in config");
        if (config.find("detection_mode") == config.end()) throw std::runtime_error("Missing 'detection_mode' in config");
        if (config.find("detection_interval") == config.end()) throw std::runtime_error("Missing 'detection_interval' in config");
        if (config.find("tracking_mode") == config.end()) throw std::runtime_error("Missing 'tracking_mode' in config");
        if (config.find("tracking_interval") == config.end()) throw std::runtime_error("Missing 'tracking_interval' in config");

        int resolutionIndex = std::stoi(config["camera_resolution_index"]);
        int customWidth = config.find("custom_width") != config.end() ? std::stoi(config["custom_width"]) : 0;
        int customHeight = config.find("custom_height") != config.end() ? std::stoi(config["custom_height"]) : 0;
        float frameRate = std::stof(config["frame_rate"]);
        int detectionMode = std::stoi(config["detection_mode"]);
        int detectionInterval = std::stoi(config["detection_interval"]);
        int trackingMode = std::stoi(config["tracking_mode"]);
        int trackingInterval = std::stoi(config["tracking_interval"]);

        // Configure camera with loaded settings
        cameraHandler.configureCamera(resolutionIndex, customWidth, customHeight);
        cameraHandler.setFrameRate(frameRate);

        // Configure detection and tracking
        controlUnit.setDetectionMode(static_cast<ControlUnit::Mode>(detectionMode));
        controlUnit.setDetectionFrameInterval(detectionInterval);
        controlUnit.setTrackingMode(static_cast<ControlUnit::Mode>(trackingMode));
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
