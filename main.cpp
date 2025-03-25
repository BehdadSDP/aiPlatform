#include "camera_handler.h"
#include "model.h"
#include <thread>
#include <atomic>

void processYoloRealtime(model& yoloDetector, std::atomic<bool>& running) {
    while (running) {
        FrameBufferManager::getInstance().waitForNewFrame();
        cv::Mat frame;
        if (FrameBufferManager::getInstance().getLatestFrame(frame)) {
            std::vector<cv::Mat> singleFrame = {frame};
            yoloDetector.detectAndDisplay(singleFrame);
        }
    }
}

void processFrames(std::atomic<bool>& running) {
    while (running) {
        std::vector<cv::Mat> batch;
        FrameBufferManager::getInstance().getAllFrames(batch);
        if (!batch.empty()) {
            std::cout << "Processing batch of " << batch.size() << " frames..." << std::endl;
            // Future AI models can process batch here
            FrameBufferManager::getInstance().clearFrames(); // Optional: clear if you want to reset
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main() {
    try {
        CameraHandler cameraHandler;
        model yoloDetector("/home/pi5/ai/models/yolov4-tiny.cfg",
                           "/home/pi5/ai/models/yolov4-tiny.weights",
                           "/home/pi5/ai/models/coco.names");
        std::atomic<bool> running(true);

        cameraHandler.initialize();
        cameraHandler.acquireCamera();
        cameraHandler.configureCamera();

        std::thread yoloThread(processYoloRealtime, std::ref(yoloDetector), std::ref(running));
        std::thread processingThread(processFrames, std::ref(running));
        cameraHandler.startStreaming();

        std::cout << "Streaming... Press Ctrl+C to exit." << std::endl;

        while (running){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        running = false;
        yoloThread.join();
        processingThread.join();
    }
    catch (const CameraException& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}
