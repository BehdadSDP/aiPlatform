#include "camera_handler.h"
#include <iomanip>
#include <sys/mman.h>

void CameraHandler::initialize() {
    if (cm_->start() < 0) {
        throw CameraException("Failed to start CameraManager");
    }
    listCameras();
}

void CameraHandler::acquireCamera(const std::string& cameraId) {
    auto cameras = cm_->cameras();
    if (cameras.empty()) {
        throw CameraException("No cameras found");
    }
    std::string id = cameraId.empty() ? cameras[0]->id() : cameraId;
    camera_ = cm_->get(id);
    if (!camera_) {
        throw CameraException("Failed to get camera with ID: " + id);
    }
    if (camera_->acquire() < 0) {
        throw CameraException("Failed to acquire camera");
    }
    std::cout << "Camera " << id << " acquired successfully!" << std::endl;
}

void CameraHandler::listCameras() const {
    auto cameras = cm_->cameras();
    if (cameras.empty()) {
        std::cout << "No cameras found." << std::endl;
        return;
    }
    std::cout << "Available cameras:" << std::endl;
    for (size_t i = 0; i < cameras.size(); ++i) {
        std::cout << i << ": " << cameras[i]->id() << std::endl;
    }
}

void CameraHandler::printMessage(const std::string& message) const {
    std::cout << message << std::endl;
}

void CameraHandler::displayOptionsSummary(const std::vector<std::pair<Size, PixelFormat>>& options, size_t customIndex) const {
    if (options.empty()) {
        printMessage("No predefined valid configuration options available.");
    } else {
        printMessage("Total predefined options: " + std::to_string(options.size()));
    }
    printMessage("\nEnter your choice (0-" + std::to_string(customIndex) + "): ");
}

template<typename T>
T CameraHandler::getUserInput(const std::string& prompt, T minValue, const std::string& errorMsg) {
    T value;
    printMessage(prompt);
    std::cout.flush();
    std::cin >> value;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (std::cin.fail() || value < minValue) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        throw CameraException(errorMsg + std::to_string(value));
    }
    return value;
}

std::vector<std::pair<Size, PixelFormat>> CameraHandler::generateConfigOptions(
        StreamConfiguration& streamConfig,
        const StreamFormats& streamFormats,
        std::unique_ptr<CameraConfiguration>& config) {
    std::vector<Size> validSizes = {{1332, 990}, {2028, 1080}, {2028, 1520}, {4056, 3040}};
    std::vector<PixelFormat> validFormats = {formats::RGB888};
    std::vector<std::pair<Size, PixelFormat>> options;
    size_t index = 0;

    printMessage("\nAvailable configuration options for " + camera_->id() + ":");
    auto supportedFormats = streamFormats.pixelformats();
    for (const auto& pixelFormat : validFormats) {
        if (std::find(supportedFormats.begin(), supportedFormats.end(), pixelFormat) == supportedFormats.end()) {
            printMessage("Format " + pixelFormat.toString() + " not supported by this stream.");
            continue;
        }
        for (const auto& size : validSizes) {
            streamConfig.size = size;
            streamConfig.pixelFormat = pixelFormat;
            auto status = config->validate();
            if (status != CameraConfiguration::Status::Invalid) {
                options.emplace_back(size, pixelFormat);
                std::cout << index++ << ": " << size.width << "x" << size.height << "-"
                          << pixelFormat.toString() << " ("
                          << (status == CameraConfiguration::Status::Valid ? "valid" : "adjusted") << ")" << std::endl;
            }
        }
    }
    std::cout << index << ": Enter custom width and height" << std::endl;
    return options;
}

void CameraHandler::configureManualResolution(StreamConfiguration& streamConfig,
                                              const std::vector<PixelFormat>& validFormats) {
    int width = getUserInput("Enter desired width: ", 1, "Invalid width entered: must be positive: ");
    int height = getUserInput("Enter desired height: ", 1, "Invalid height entered: must be positive: ");
    printMessage("Attempting custom resolution: " + std::to_string(width) + "x" + std::to_string(height));
    streamConfig.size = Size{static_cast<unsigned int>(width), static_cast<unsigned int>(height)};
    streamConfig.pixelFormat = validFormats[0];
}

void CameraHandler::applyConfiguration(std::unique_ptr<CameraConfiguration>& config,
                                       StreamConfiguration& streamConfig) {
    auto status = config->validate();
    printMessage("Validation status: " + std::string(
                     status == CameraConfiguration::Status::Valid ? "Valid" :
                                                                    status == CameraConfiguration::Status::Adjusted ? "Adjusted" : "Invalid"));
    if (status == CameraConfiguration::Status::Invalid) {
        throw CameraException("Configuration " + streamConfig.toString() + " is not supported");
    }
    printMessage("Configuring camera with: " + streamConfig.toString());
    if (camera_->configure(config.get()) < 0) {
        throw CameraException("Failed to configure camera with " + streamConfig.toString());
    }
    printMessage("Applied configuration: " + streamConfig.toString() +
                 (config->validate() == CameraConfiguration::Status::Adjusted ? " (adjusted)" : ""));
    stream_ = config->at(0).stream();
}

void CameraHandler::configureCamera(int resolutionIndex, int customWidth, int customHeight) {
    if (!camera_) throw CameraException("Camera not acquired yet. Call acquireCamera() first.");
    auto config = camera_->generateConfiguration({StreamRole::Viewfinder});
    if (!config) throw CameraException("Failed to generate configuration for Viewfinder role.");
    StreamConfiguration& streamConfig = config->at(0);
    printMessage("Default configuration: " + streamConfig.toString());

    const StreamFormats& streamFormats = streamConfig.formats();
    auto options = generateConfigOptions(streamConfig, streamFormats, config);
    size_t customIndex = options.size();

    if (resolutionIndex == static_cast<int>(customIndex)) {
        printMessage("Attempting custom resolution: " + std::to_string(customWidth) + "x" + std::to_string(customHeight));
        streamConfig.size = Size{static_cast<unsigned int>(customWidth), static_cast<unsigned int>(customHeight)};
        streamConfig.pixelFormat = formats::RGB888;
    } else if (resolutionIndex >= 0 && resolutionIndex < static_cast<int>(options.size())) {
        streamConfig.size = options[resolutionIndex].first;
        streamConfig.pixelFormat = options[resolutionIndex].second;
    } else {
        throw CameraException("Invalid resolution index: " + std::to_string(resolutionIndex));
    }

    applyConfiguration(config, streamConfig);
}

void CameraHandler::setFrameRate(int targetFps) {
    if (!camera_) {
        throw CameraException("Camera not acquired yet. Call acquireCamera() first.");
    }
    if (targetFps <= 0) {
        throw CameraException("Invalid frame rate: " + std::to_string(targetFps) + ". Must be positive.");
    }
    frameDuration_ = static_cast<int64_t>(1'000'000 / targetFps);
    printMessage("Requested frame rate: " + std::to_string(targetFps) + " FPS (Frame duration: " +
                 std::to_string(frameDuration_) + " µs)");
}

void CameraHandler::cleanup() {
    stopStreaming();
    if (camera_) {
        camera_->release();
        camera_.reset();
    }
    if (cm_) {
        cm_->stop();
        cm_.reset();
    }
    stream_ = nullptr;
    frameDuration_ = 0;
}

void CameraHandler::startStreaming() {
    if (!camera_ || !stream_) {
        throw CameraException("Camera or stream not configured yet");
    }

    allocator_ = std::make_unique<FrameBufferAllocator>(camera_);
    if (allocator_->allocate(stream_) < 0) {
        throw CameraException("Failed to allocate frame buffers");
    }

    const auto& buffers = allocator_->buffers(stream_);
    for (const auto& buffer : buffers) {
        std::unique_ptr<Request> request = camera_->createRequest();
        if (!request || request->addBuffer(stream_, buffer.get()) < 0) {
            throw CameraException("Failed to create/add buffer to request");
        }
        if (frameDuration_ > 0) {
            int64_t minDuration = frameDuration_ * 0.95;
            int64_t maxDuration = frameDuration_ * 1.05;
            int64_t durationRange[2] = {minDuration, maxDuration};
            request->controls().set(controls::FrameDurationLimits, Span<const int64_t, 2>(durationRange, 2));
        }
        requests_.push_back(std::move(request));
    }

    camera_->requestCompleted.connect(this, &CameraHandler::requestComplete);

    if (camera_->start() < 0) {
        throw CameraException("Failed to start camera");
    }
    for (auto& request : requests_) {
        if (camera_->queueRequest(request.get()) < 0) {
            throw CameraException("Failed to queue request");
        }
    }
    printMessage("Camera streaming started. Press Ctrl+C to stop.");
}

void CameraHandler::stopStreaming() {
    if (camera_ && !requests_.empty()) {
        camera_->stop();
        requests_.clear();
        allocator_.reset();
        printMessage("Camera streaming stopped.");
    }
}

void CameraHandler::requestComplete(Request* request) {
    if (request->status() != Request::RequestComplete) {
        std::cerr << "Request failed" << std::endl;
        return;
    }

    auto currentTime = std::chrono::steady_clock::now();
    double timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(
                currentTime - lastFrameTime_).count() / 1e6;
    lastFrameTime_ = currentTime;
    if (timeDiff > 0) {
        fps_ = 1.0 / timeDiff;
    }

    const auto& buffers = request->buffers();
    for (const auto& [stream, buffer] : buffers) {
        const FrameMetadata& metadata = buffer->metadata();
        const StreamConfiguration& config = stream->configuration();

        const auto& planes = buffer->planes();
        int fd = planes[0].fd.get();
        size_t length = planes[0].length;
        void* mappedData = mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
        if (mappedData == MAP_FAILED) {
            std::cerr << "Failed to map buffer" << std::endl;
            continue;
        }

        uint8_t* data = static_cast<uint8_t*>(mappedData);
        cv::Mat rawFrame(config.size.height, config.size.width, CV_8UC3, data, config.stride);

        // Create FrameData object
        FrameData frameData;
        frameData.image = rawFrame.clone();
        frameData.timestamp = metadata.timestamp;
        frameData.sequence = metadata.sequence;
        frameData.format = config.pixelFormat.toString();
        frameData.size = cv::Size(config.size.width, config.size.height);
        frameData.fps = fps_;

        // Add to buffer
        FrameBufferManager::getInstance().addFrame(frameData);

        // Increment both counters here
        controlUnit_.incrementDetectionFrameCounter();

        munmap(mappedData, length);
    }

    request->reuse(Request::ReuseBuffers);
    camera_->queueRequest(request);
}
