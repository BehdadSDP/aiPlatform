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

static constexpr float CONF_THRESHOLD = 0.5f;

std::atomic<bool>* g_running = nullptr;
void signalHandler(int) { if (g_running) g_running->store(false); }

void threadYolo(model &yoloDetector, SingleObjectData &sharedData, std::atomic<bool> &running, ControlUnit& controlUnit)
{
    while (running) {
        FrameBufferManager::getInstance().waitForNewFrame();

        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) {
            continue;
        }

        // Check mode before processing
        if (controlUnit.getDetectionMode() == ControlUnit::Mode::RUN && controlUnit.shouldDetect()) {
            std::vector<model::Detection> detections = yoloDetector.detect(frame);

            float bestConf = -1.0f;
            cv::Rect bestBox;
            for (auto &det : detections) {
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
                } else {
                    sharedData.detection.valid = false;
                }
                sharedData.detection.frameSeq = frameData.sequence;
            }
        }
        // In STANDBY, do nothing but keep looping
    }
}

void threadTracker(SingleObjectData &sharedData, std::atomic<bool> &running, ControlUnit& controlUnit)
{
    std::string vitModelPath = "/home/pi5/ai_platform/aiPlatform/models/object_tracking_vittrack_2023sep.onnx";
    std::unique_ptr<VitTracker> tracker;
    bool isTracking = false;
    int framesWithoutDetection = 0;
    const int maxFramesWithoutDetection = 30;

    while (running) {
        FrameBufferManager::getInstance().waitForNewFrame();

        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) {
            continue;
        }

        // Check mode before processing
        if (controlUnit.getTrackingMode() == ControlUnit::Mode::RUN && controlUnit.shouldTrack()) {
            cv::Rect lastTrackBox;
            bool trackerValid = false;

            if (tracker && isTracking) {
                lastTrackBox = tracker->update(frame);
                trackerValid = lastTrackBox.width > 0 && lastTrackBox.height > 0 && tracker->isInitialized();
            }

            ControlUnit::Action action = controlUnit.decideAction(sharedData, isTracking, lastTrackBox);

            switch (action) {
                case ControlUnit::Action::INITIALIZE: {
                    cv::Rect yoloBox;
                    {
                        std::lock_guard<std::mutex> lock(sharedData.mtx);
                        yoloBox = sharedData.detection.box;
                    }
                    tracker = std::make_unique<VitTracker>(vitModelPath);
                    tracker->init(frame, yoloBox);
                    isTracking = true;
                    framesWithoutDetection = 0;
                    break;
                }
                case ControlUnit::Action::REINITIALIZE: {
                    cv::Rect yoloBox;
                    {
                        std::lock_guard<std::mutex> lock(sharedData.mtx);
                        yoloBox = sharedData.detection.box;
                    }
                    tracker->init(frame, yoloBox);
                    isTracking = true;
                    framesWithoutDetection = 0;
                    break;
                }
                case ControlUnit::Action::CONTINUE:
                    if (!trackerValid) {
                        isTracking = false;
                        tracker.reset();
                    }
                    break;
                case ControlUnit::Action::STOP:
                    isTracking = false;
                    tracker.reset();
                    break;
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
                cv::rectangle(frame, lastTrackBox, cv::Scalar(0,0,255), 2);
                cv::putText(frame, "Tracking", lastTrackBox.tl(),
                            cv::FONT_HERSHEY_SIMPLEX, 0.6,
                            cv::Scalar(0,0,255), 2);
            }
        } else {
            // In STANDBY, reset tracking state and skip processing
            isTracking = false;
            tracker.reset();
            framesWithoutDetection = 0;
        }

        cv::imshow("Tracker Thread View", frame);
        cv::waitKey(1);
    }
}

int main()
{
    try {
        CameraHandler cameraHandler;
        model yoloDetector("/home/pi5/ai_platform/aiPlatform/models/yolov4-tiny.cfg",
                           "/home/pi5/ai_platform/aiPlatform/models/yolov4-tiny.weights",
                           "/home/pi5/ai_platform/aiPlatform/models/coco.names");

        std::atomic<bool> running(true);
        ControlUnit controlUnit;
        g_running = &running;
        signal(SIGINT, signalHandler);

        cameraHandler.initialize();
        cameraHandler.acquireCamera();
        cameraHandler.configureCamera();

        // Prompt for detection mode and interval
        std::cout << "Select Detection Mode:\n0: RUN\n1: STANDBY\nEnter choice (0-1): ";
        int detectionChoice;
        std::cin >> detectionChoice;
        controlUnit.setDetectionMode(static_cast<ControlUnit::Mode>(detectionChoice));

        int detectionInterval = 1;
        if (detectionChoice == 0) {
            std::cout << "Enter frame interval for detection (e.g., 10 for every 10th frame): ";
            std::cin >> detectionInterval;
            controlUnit.setDetectionFrameInterval(detectionInterval);
        }

        // Prompt for tracking mode and interval
        std::cout << "Select Tracking Mode:\n0: RUN\n1: STANDBY\nEnter choice (0-1): ";
        int trackingChoice;
        std::cin >> trackingChoice;
        controlUnit.setTrackingMode(static_cast<ControlUnit::Mode>(trackingChoice));

        int trackingInterval = 1;
        if (trackingChoice == 0) {
            std::cout << "Enter frame interval for tracking (e.g., 5 for every 5th frame): ";
            std::cin >> trackingInterval;
            controlUnit.setTrackingFrameInterval(trackingInterval);
        }

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
