#include "camera_handler.h"
#include "model.h"
#include "vittracker.h"
#include "shared_data.h"
#include <thread>
#include <atomic>

static constexpr float CONF_THRESHOLD = 0.5f; // Minimum YOLO confidence for your object

void threadYolo(model &yoloDetector, SingleObjectData &sharedData, std::atomic<bool> &running)
{
    while (running) {
        // Wait for new frame
        FrameBufferManager::getInstance().waitForNewFrame();

        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) {
            continue;
        }

        // 1) Run YOLO to get the best bounding box for one object
        std::vector<model::Detection> detections = yoloDetector.detect(frame);

        float bestConf = -1.0f;
        cv::Rect bestBox;
        for (auto &det : detections) {
            if (det.confidence > bestConf) {
                bestConf = det.confidence;
                bestBox  = det.box;
            }
        }

        // 2) Write the best bounding box into sharedData
        {
            std::lock_guard<std::mutex> lock(sharedData.mtx);
            if (bestConf > 0.5f) {
                sharedData.detection.box = bestBox;
                sharedData.detection.valid = true;
            } else {
                // If YOLO didn't see anything, mark invalid
                sharedData.detection.valid = false;
            }
            // Store the frame sequence or timestamp
            sharedData.detection.frameSeq = frameData.sequence;
        }

        // (Optional) you can also draw and show the YOLO bounding box if desired
         cv::rectangle(frame, bestBox, cv::Scalar(0,255,0), 2);
         cv::imshow("YOLO View", frame);
         cv::waitKey(1);
    }
}

void threadTracker(SingleObjectData &sharedData, std::atomic<bool> &running)
{
    // Path to your ONNX tracker model
    std::string vitModelPath = "/home/pi5/ai_platform/aiPlatform/models/object_tracking_vittrack_2023sep.onnx";
    std::unique_ptr<VitTracker> tracker;
    bool isTracking = false;

    while (running) {
        // Wait for new frame
        FrameBufferManager::getInstance().waitForNewFrame();

        FrameData frameData;
        if (!FrameBufferManager::getInstance().getLatestFrame(frameData)) {
            continue;
        }
        cv::Mat frame = frameData.image;
        if (frame.empty()) {
            continue;
        }

        // 1) Grab the YOLO bounding box from shared data
        cv::Rect yoloBox;
        bool yoloValid = false;
        {
            std::lock_guard<std::mutex> lock(sharedData.mtx);
            yoloValid = sharedData.detection.valid;
            if (yoloValid) {
                yoloBox = sharedData.detection.box;
            }
        }

        // 2) If not tracking, and YOLO is valid, initialize the tracker
        if (!isTracking && yoloValid) {
            tracker = std::make_unique<VitTracker>(vitModelPath);
            tracker->init(frame, yoloBox);
            isTracking = true;
        }
        else if (isTracking) {
            // 3) If we are tracking, call update
            cv::Rect trackBox = tracker->update(frame);

            // If the tracker fails or returns invalid box, reset
            if (trackBox.width <= 0 || trackBox.height <= 0) {
                isTracking = false;
                tracker.reset();
            }
            else {
                // (Optional) "Refresh" the tracker with YOLO's bounding box
                // if YOLO is valid. This prevents drift.
                if (yoloValid) {
                    // For example, if you want to re-init if YOLO is confident:
                    tracker->init(frame, yoloBox);
                }

                // Draw the tracked box
                cv::rectangle(frame, trackBox, cv::Scalar(0,0,255), 2);
                cv::putText(frame, "Tracking", trackBox.tl(),
                            cv::FONT_HERSHEY_SIMPLEX, 0.6,
                            cv::Scalar(0,0,255), 2);
            }
        }

        // (Optional) Show the tracker result
        cv::imshow("Tracker Thread View", frame);
        cv::waitKey(1);
    }
}

int main()
{
    try {
        CameraHandler cameraHandler;
        model yoloDetector("/home/pi5/ai_platform/aiPlatform/models/yolov4-tiny.cfg",
                           "//home/pi5/ai_platform/aiPlatform/models/yolov4-tiny.weights",
                           "/home/pi5/ai_platform/aiPlatform/models/coco.names");

        std::atomic<bool> running(true);

        // Initialize camera
        cameraHandler.initialize();
        cameraHandler.acquireCamera();
        cameraHandler.configureCamera();
        cameraHandler.startStreaming();

        // Create shared data
        SingleObjectData singleObjData;
        {
            std::lock_guard<std::mutex> lock(singleObjData.mtx);
            singleObjData.detection.valid = false; // start invalid
            singleObjData.detection.frameSeq = 0;
        }

        // Start YOLO thread
        std::thread yoloThread(threadYolo, std::ref(yoloDetector), std::ref(singleObjData), std::ref(running));

        // Start Tracker thread
        std::thread trackerThread(threadTracker, std::ref(singleObjData), std::ref(running));

        std::cout << "Streaming... Press Ctrl+C to exit." << std::endl;

        // Wait or watch for signals to stop
        while (/*some condition*/ true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // If user hits Ctrl+C or some break condition, set running=false
        }

        running = false;
        yoloThread.join();
        trackerThread.join();
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
