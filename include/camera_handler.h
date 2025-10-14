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
#include "frame_buffer_manager.h"
#include "control_unit.h" // Include ControlUnit

using namespace libcamera;

class CameraException : public std::runtime_error {
public:
    explicit CameraException(const std::string& message)
        : std::runtime_error("Camera Error: " + message) {}
};

class CameraHandler {
public:
    // Updated constructor to take ControlUnit reference
    CameraHandler(ControlUnit& controlUnit)
        : cm_(std::make_unique<CameraManager>()), camera_(nullptr), stream_(nullptr),
          controlUnit_(controlUnit), // Store reference
          lastFrameTime_(std::chrono::steady_clock::now()), frameCount_(0), fps_(0.0),
          frameDuration_(0), rotationAngle_(0), 
          exposureTimeUs_(0), autoExposure_(true),
          deblurEnabled_(false), deblurMethod_(0), deblurStrength_(0.5),
          mappedBuffers_(8) {} // Pre-allocate for performance

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

    void configureCamera(int resolutionIndex, int customWidth = 0, int customHeight = 0);
    void setFrameRate(int targetFps);
    void setRotation(int rotationAngle);
    void setExposureTime(int exposureTimeUs);  // Set exposure time in microseconds
    void setAutoExposure(bool enable = true);  // Enable/disable auto exposure
    
    // Deblurring methods for drone vibration compensation
    void enableDeblur(bool enable = true);
    void setDeblurMethod(int method);  // 0=None, 1=Gaussian, 2=Wiener, 3=Blind Deconvolution, 4=Sharpening
    void setDeblurStrength(double strength);  // 0.0-1.0

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
    ControlUnit& controlUnit_; // Reference to ControlUnit

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
    int rotationAngle_;
    
    // Exposure control parameters
    int exposureTimeUs_;     // Exposure time in microseconds
    bool autoExposure_;      // Auto exposure enabled
    
    // Deblurring parameters
    bool deblurEnabled_;
    int deblurMethod_;      // 0=None, 1=Gaussian, 2=Wiener, 3=Blind, 4=Sharpening
    double deblurStrength_; // 0.0-1.0
    
    // Performance optimizations
    struct MappedBuffer {
        void* ptr = nullptr;
        size_t length = 0;
        int fd = -1;
        bool active = false;
    };
    std::vector<MappedBuffer> mappedBuffers_;
    void* mapBuffer(int fd, size_t length, int bufferIndex);
    void unmapBuffer(int bufferIndex);
    void cleanupMappedBuffers();
    cv::Mat rotateImage(const cv::Mat& inputImage);
    cv::Mat deblurFrame(const cv::Mat& blurredFrame);
    cv::Mat applyGaussianDeblur(const cv::Mat& frame);
    cv::Mat applyWienerDeblur(const cv::Mat& frame);
    cv::Mat applyBlindDeconvolution(const cv::Mat& frame);
    cv::Mat applySharpeningFilter(const cv::Mat& frame);
};

#endif // CAMERA_HANDLER_H
