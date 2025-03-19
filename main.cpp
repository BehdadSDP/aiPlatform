#include "camera_handler.h"
#include "dataloader.h"
#include "model.h"
#include <thread>
#include <atomic>

void processYoloRealtime(model& yoloDetector, std::atomic<bool>& running) {
    while (running) {
        FrameBufferManager::getInstance().waitForNewFrame(); // Wait for a new frame
        cv::Mat frame;
        if (FrameBufferManager::getInstance().getLatestFrame(frame)) {
            std::vector<cv::Mat> singleFrame = {frame};
            yoloDetector.detectAndDisplay(singleFrame); // Process immediately
        }
    }
}

void processFrames(DataLoader& dataLoader) {
    while (true) {
        std::vector<cv::Mat> batch;
        if (!dataLoader.getBatch(batch)) {
            break;
        }
        std::cout << "Processing batch of " << batch.size() << " frames in DataLoader..." << std::endl;
        // Future AI models can process batch here
    }
}

int main() {
    try {
        CameraHandler cameraHandler;
        DataLoader dataLoader(500);
        model yoloDetector("/home/pi5/ai/models/yolov4-tiny.cfg",
                           "/home/pi5/ai/models/yolov4-tiny.weights",
                           "/home/pi5/ai/models/coco.names");
        std::atomic<bool> running(true);

        cameraHandler.initialize();
        cameraHandler.acquireCamera();
        cameraHandler.configureCamera();

        std::thread yoloThread(processYoloRealtime, std::ref(yoloDetector), std::ref(running));
        std::thread processingThread(processFrames, std::ref(dataLoader));

        cameraHandler.startStreaming();

        std::cout << "Streaming... Press Ctrl+C to exit." << std::endl;
        while (true) {
            if (cameraHandler.isBufferFull()) {
                std::vector<cv::Mat> frames;
                cameraHandler.getBufferedFrames(frames);
                for (const auto& frame : frames) {
                    dataLoader.addFrame(frame);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        running = false;
        dataLoader.stop();
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
