#include "include/camera_handler.h"
#include <iomanip>
#include <sys/mman.h>
#include <iostream>

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
    
    // Find camera by ID instead of using get() method
    std::shared_ptr<Camera> selectedCamera = nullptr;
    for (const auto& camera : cameras) {
        if (camera->id() == id) {
            selectedCamera = camera;
            break;
        }
    }
    
    if (!selectedCamera) {
        throw CameraException("Failed to get camera with ID: " + id);
    }
    
    camera_ = selectedCamera;
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
    cleanupMappedBuffers();
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
    static int totalFrames = 0;
    totalFrames++;
    
    // Debug: Print every 100 frames to ensure this function is being called
    if (totalFrames % 100 == 0) {
        std::cout << "DEBUG: Processed " << totalFrames << " frames total" << std::endl;
    }
    
    if (!request) {
        std::cerr << "Null request received" << std::endl;
        return;
    }
    
    if (request->status() != Request::RequestComplete) {
        std::cerr << "Request failed with status: " << request->status() << std::endl;
        // Still try to reuse the request
        request->reuse(Request::ReuseBuffers);
        camera_->queueRequest(request);
        return;
    }

    const auto& buffers = request->buffers();
    for (const auto& [stream, buffer] : buffers) {
        if (!stream || !buffer) {
            std::cerr << "Invalid stream or buffer" << std::endl;
            continue;
        }
        
        const FrameMetadata& metadata = buffer->metadata();
        const StreamConfiguration& config = stream->configuration();

        const auto& planes = buffer->planes();
        if (planes.empty()) {
            std::cerr << "No planes in buffer" << std::endl;
            continue;
        }
        
        // Find buffer index by matching the FrameBuffer pointer
        int bufferIndex = -1;
        if (allocator_) {
            // Cast away const - safe since we're only reading from allocator
            Stream* nonConstStream = const_cast<Stream*>(stream);
            const auto& allocatedBuffers = allocator_->buffers(nonConstStream);
            for (size_t i = 0; i < allocatedBuffers.size() && i < mappedBuffers_.size(); ++i) {
                if (allocatedBuffers[i].get() == buffer) {
                    bufferIndex = static_cast<int>(i);
                    break;
                }
            }
        }
        
        // Use optimized buffer mapping with proper index
        int fd = planes[0].fd.get();
        size_t length = planes[0].length;
        void* mappedData = mapBuffer(fd, length, bufferIndex);
        if (mappedData == MAP_FAILED || mappedData == nullptr) {
            std::cerr << "Failed to map buffer" << std::endl;
            continue;
        }

        uint8_t* data = static_cast<uint8_t*>(mappedData);
        cv::Mat rawFrame(config.size.height, config.size.width, CV_8UC3, data, config.stride);

        // Create FrameData object - MUST clone for memory safety
        FrameData frameData;
        frameData.image = rawFrame.clone();  // Essential: buffer will be reused
        frameData.timestamp = metadata.timestamp;
        frameData.sequence = metadata.sequence;
        
        // Cache format string to avoid repeated conversions
        static std::string cachedFormat;
        static PixelFormat lastFormat;
        if (config.pixelFormat != lastFormat) {
            cachedFormat = config.pixelFormat.toString();
            lastFormat = config.pixelFormat;
        }
        frameData.format = cachedFormat;
        
        frameData.size = cv::Size(config.size.width, config.size.height);
        frameData.fps = fps_;

        // Add to buffer
        FrameBufferManager::getInstance().addFrame(frameData);

        // Increment camera counter
        controlUnit_.notifyNewFrame();

        // Keep mapping for reuse (don't unmap)
        unmapBuffer(bufferIndex);
    }

    request->reuse(Request::ReuseBuffers);
    camera_->queueRequest(request);
}

void* CameraHandler::mapBuffer(int fd, size_t length, int bufferIndex) {
    if (bufferIndex >= 0 && bufferIndex < static_cast<int>(mappedBuffers_.size())) {
        auto& buffer = mappedBuffers_[bufferIndex];
        
        // Reuse existing mapping if same fd and length
        if (buffer.active && buffer.fd == fd && buffer.length == length) {
            return buffer.ptr;
        }
        
        // Unmap old buffer if exists
        if (buffer.active) {
            munmap(buffer.ptr, buffer.length);
            buffer.active = false;
            buffer.ptr = nullptr;
        }
        
        // Create new mapping
        buffer.ptr = mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
        if (buffer.ptr != MAP_FAILED) {
            buffer.fd = fd;
            buffer.length = length;
            buffer.active = true;
            return buffer.ptr;
        } else {
            std::cerr << "Failed to map buffer: " << strerror(errno) << std::endl;
            return nullptr;
        }
    }
    
    // Fallback to regular mmap with proper tracking
    void* ptr = mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Fallback mmap failed: " << strerror(errno) << std::endl;
        return nullptr;
    }
    
    // Track this mapping for cleanup
    MappedBuffer tempBuffer;
    tempBuffer.ptr = ptr;
    tempBuffer.length = length;
    tempBuffer.fd = fd;
    tempBuffer.active = true;
    mappedBuffers_.push_back(tempBuffer);
    
    return ptr;
}

void CameraHandler::unmapBuffer(int bufferIndex) {
    if (bufferIndex >= 0 && bufferIndex < static_cast<int>(mappedBuffers_.size())) {
        auto& buffer = mappedBuffers_[bufferIndex];
        if (buffer.active) {
            // Keep mapping for reuse - don't unmap
            // buffer.active = false;
        }
    }
}

void CameraHandler::cleanupMappedBuffers() {
    for (auto& buffer : mappedBuffers_) {
        if (buffer.active && buffer.ptr != nullptr) {
            munmap(buffer.ptr, buffer.length);
            buffer.active = false;
            buffer.ptr = nullptr;
        }
    }
    mappedBuffers_.clear();
}

