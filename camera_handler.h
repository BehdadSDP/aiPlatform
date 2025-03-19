#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include <iostream>
#include <libcamera/libcamera.h>
#include <libcamera/control_ids.h>
#include <libcamera/controls.h>
#include <memory>
#include <stdexcept>
#include <vector>
#include <limits>
#include <set>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <opencv4/opencv2/opencv.hpp>
#include "model.h"
#include "frame_buffer_manager.h"

using namespace libcamera;

class CameraException : public std::runtime_error {
public:
    explicit CameraException(const std::string& message)
        : std::runtime_error("Camera Error: " + message) {}
};

class CameraHandler {
public:
    CameraHandler()
        : cm_(std::make_unique<CameraManager>()), camera_(nullptr), stream_(nullptr),
          lastFrameTime_(std::chrono::steady_clock::now()), frameCount_(0), fps_(0.0),
          frameDuration_(0) {}

    ~CameraHandler() { cleanup(); }

    void initialize();
    void acquireCamera(const std::string& cameraId = "");
    void listCameras() const;
    void printMessage(const std::string& message) const;
    void displayOptionsSummary(const std::vector<std::pair<Size, PixelFormat>>& options, size_t customIndex) const;

    template<typename T>
    T getUserInput(const std::string& prompt, T minValue, const std::string& errorMsg);

    std::vector<std::pair<Size, PixelFormat>> generateConfigOptions(
            StreamConfiguration& streamConfig,
            const StreamFormats& streamFormats,
            std::unique_ptr<CameraConfiguration>& config);

    void configureManualResolution(StreamConfiguration& streamConfig,
                                   const std::vector<PixelFormat>& validFormats);
    void applyConfiguration(std::unique_ptr<CameraConfiguration>& config,
                            StreamConfiguration& streamConfig);

    void configureCamera();
    void setFrameRate(float targetFps);

    void cleanup();
    void startStreaming();
    void stopStreaming();

    std::shared_ptr<Camera> getCamera() const { return camera_; }
    void requestComplete(Request* request);

    void getBufferedFrames(std::vector<cv::Mat>& outputFrames);
    bool isBufferFull() const { return FrameBufferManager::getInstance().size() >= 500; }

private:
    std::unique_ptr<CameraManager> cm_;
    std::shared_ptr<Camera> camera_;
    Stream* stream_ = nullptr;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool frameCaptured_ = false;

    Size capturedSize_;
    PixelFormat capturedFormat_;
    size_t capturedBufferSize_ = 0;
    unsigned int capturedStride_ = 0;

    std::unique_ptr<FrameBufferAllocator> allocator_;
    std::vector<std::unique_ptr<Request>> requests_;

    std::chrono::steady_clock::time_point lastFrameTime_;
    unsigned int frameCount_;
    double fps_;
    int64_t frameDuration_;
};

#endif // CAMERA_HANDLER_H
