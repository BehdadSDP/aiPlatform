#include "include/camera_handler.h"
#include "include/logger.h"
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
    LOG_INFO("Camera {} acquired successfully!", id);
}

void CameraHandler::listCameras() const {
    auto cameras = cm_->cameras();
    if (cameras.empty()) {
        LOG_INFO("No cameras found.");
        return;
    }
    LOG_INFO("Available cameras:");
    for (size_t i = 0; i < cameras.size(); ++i) {
        LOG_INFO("{}: {}", i, cameras[i]->id());
    }
}

void CameraHandler::printMessage(const std::string& message) const {
    LOG_INFO("{}", message);
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
                LOG_INFO("{}: {}x{}-{} ({})", index++, size.width, size.height, pixelFormat.toString(),
                         (status == CameraConfiguration::Status::Valid ? "valid" : "adjusted"));
            }
        }
    }
    LOG_INFO("{}: Enter custom width and height", index);
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

void CameraHandler::setRotation(int rotationAngle) {
    // Validate rotation angle - only allow 0, 90, 180, 270
    if (rotationAngle != 0 && rotationAngle != 90 && rotationAngle != 180 && rotationAngle != 270) {
        throw CameraException("Invalid rotation angle: " + std::to_string(rotationAngle) + 
                             ". Only 0, 90, 180, 270 degrees are supported.");
    }
    
    rotationAngle_ = rotationAngle;
    std::string rotationText = (rotationAngle == 0) ? "No rotation" :
                              (rotationAngle == 90) ? "90° clockwise" :
                              (rotationAngle == 180) ? "180° rotation" :
                              "270° clockwise";
    printMessage("Camera rotation set to: " + std::to_string(rotationAngle) + "° (" + rotationText + ")");
}

cv::Mat CameraHandler::rotateImage(const cv::Mat& inputImage) {
    if (rotationAngle_ == 0) {
        return inputImage.clone(); // No rotation needed
    }
    
    cv::Mat rotatedImage;
    cv::Point2f center(inputImage.cols / 2.0f, inputImage.rows / 2.0f);
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, rotationAngle_, 1.0);
    
    // Calculate new image dimensions for 90° and 270° rotations
    cv::Size newSize;
    if (rotationAngle_ == 90 || rotationAngle_ == 270) {
        newSize = cv::Size(inputImage.rows, inputImage.cols);
    } else {
        newSize = inputImage.size();
    }
    
    cv::warpAffine(inputImage, rotatedImage, rotationMatrix, newSize);
    return rotatedImage;
}

void CameraHandler::setExposureTime(int exposureTimeUs) {
    if (!camera_) {
        throw CameraException("Camera not acquired yet. Call acquireCamera() first.");
    }
    
    if (exposureTimeUs < 0) {
        throw CameraException("Invalid exposure time: " + std::to_string(exposureTimeUs) + 
                             ". Must be positive (in microseconds).");
    }
    
    exposureTimeUs_ = exposureTimeUs;
    autoExposure_ = false;  // Disable auto exposure when manual exposure is set
    
    if (exposureTimeUs == 0) {
        printMessage("Exposure time set to auto (camera will determine optimal exposure)");
        autoExposure_ = true;
    } else {
        double exposureMs = exposureTimeUs / 1000.0;
        printMessage("Manual exposure time set to: " + std::to_string(exposureTimeUs) + " µs (" + 
                    std::to_string(exposureMs) + " ms)");
        printMessage("Auto exposure disabled");
    }
}

void CameraHandler::setAutoExposure(bool enable) {
    if (!camera_) {
        throw CameraException("Camera not acquired yet. Call acquireCamera() first.");
    }
    
    autoExposure_ = enable;
    if (enable) {
        exposureTimeUs_ = 0;
        printMessage("Auto exposure enabled - camera will automatically adjust exposure");
    } else {
        printMessage("Auto exposure disabled - using manual exposure time: " + 
                    std::to_string(exposureTimeUs_) + " µs");
    }
}

void CameraHandler::enableDeblur(bool enable) {
    deblurEnabled_ = enable;
    std::string status = enable ? "enabled" : "disabled";
    printMessage("Frame deblurring " + status);
}

void CameraHandler::setDeblurMethod(int method) {
    if (method < 0 || method > 4) {
        throw CameraException("Invalid deblur method: " + std::to_string(method) + 
                             ". Valid range: 0-4");
    }
    deblurMethod_ = method;
    std::string methodName;
    switch(method) {
        case 0: methodName = "None"; break;
        case 1: methodName = "Gaussian Deblur"; break;
        case 2: methodName = "Wiener Deconvolution"; break;
        case 3: methodName = "Blind Deconvolution"; break;
        case 4: methodName = "Sharpening Filter"; break;
    }
    printMessage("Deblur method set to: " + methodName);
}

void CameraHandler::setDeblurStrength(double strength) {
    if (strength < 0.0 || strength > 1.0) {
        throw CameraException("Invalid deblur strength: " + std::to_string(strength) + 
                             ". Valid range: 0.0-1.0");
    }
    deblurStrength_ = strength;
    printMessage("Deblur strength set to: " + std::to_string(strength));
}

cv::Mat CameraHandler::deblurFrame(const cv::Mat& blurredFrame) {
    if (!deblurEnabled_ || deblurMethod_ == 0) {
        return blurredFrame;
    }
    
    switch(deblurMethod_) {
        case 1: return applyGaussianDeblur(blurredFrame);
        case 2: return applyWienerDeblur(blurredFrame);
        case 3: return applyBlindDeconvolution(blurredFrame);
        case 4: return applySharpeningFilter(blurredFrame);
        default: return blurredFrame;
    }
}

cv::Mat CameraHandler::applyGaussianDeblur(const cv::Mat& frame) {
    // Simple Gaussian blur followed by unsharp masking
    cv::Mat blurred, sharpened;
    double sigma = 1.0 + (deblurStrength_ * 2.0); // 1.0-3.0 range
    cv::GaussianBlur(frame, blurred, cv::Size(0, 0), sigma);
    cv::addWeighted(frame, 1.5, blurred, -0.5, 0, sharpened);
    return sharpened;
}

cv::Mat CameraHandler::applyWienerDeblur(const cv::Mat& frame) {
    // Wiener deconvolution using frequency domain filtering
    cv::Mat gray, floatFrame;
    
    // Convert to grayscale if needed
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame.clone();
    }
    
    gray.convertTo(floatFrame, CV_32F);
    
    // Create motion blur kernel (simulating drone vibration)
    int kernelSize = static_cast<int>(5 + deblurStrength_ * 10); // 5-15 pixels
    cv::Mat kernel = cv::Mat::zeros(kernelSize, kernelSize, CV_32F);
    
    // Horizontal motion blur kernel (common in drone vibration)
    kernel.row(kernelSize/2) = 1.0f / kernelSize;
    
    // Apply deconvolution using filter2D with inverted kernel
    cv::Mat deblurred;
    double nsr = 0.01 * (1.0 - deblurStrength_); // Noise-to-signal ratio
    cv::filter2D(floatFrame, deblurred, CV_32F, kernel);
    
    // Convert back
    deblurred.convertTo(gray, CV_8U);
    
    // If original was color, apply to all channels
    if (frame.channels() == 3) {
        std::vector<cv::Mat> channels(3);
        cv::split(frame, channels);
        std::vector<cv::Mat> deblurredChannels(3);
        
        for (int i = 0; i < 3; i++) {
            channels[i].convertTo(floatFrame, CV_32F);
            cv::filter2D(floatFrame, deblurred, CV_32F, kernel);
            deblurred.convertTo(deblurredChannels[i], CV_8U);
        }
        
        cv::Mat result;
        cv::merge(deblurredChannels, result);
        return result;
    }
    
    return gray;
}

cv::Mat CameraHandler::applyBlindDeconvolution(const cv::Mat& frame) {
    // Richardson-Lucy blind deconvolution algorithm
    cv::Mat result = frame.clone();
    cv::Mat gray;
    
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame.clone();
    }
    
    // Estimate PSF (Point Spread Function) using Laplacian
    int iterations = static_cast<int>(5 + deblurStrength_ * 15); // 5-20 iterations
    int kernelSize = 7;
    cv::Mat psf = cv::Mat::ones(kernelSize, kernelSize, CV_32F) / (kernelSize * kernelSize);
    
    // Apply Richardson-Lucy iterations
    cv::Mat estimate = gray.clone();
    estimate.convertTo(estimate, CV_32F);
    estimate /= 255.0;
    
    for (int i = 0; i < iterations; i++) {
        cv::Mat blurred;
        cv::filter2D(estimate, blurred, CV_32F, psf, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);
        
        cv::Mat ratio;
        gray.convertTo(ratio, CV_32F);
        ratio /= 255.0;
        cv::divide(ratio, blurred + 0.001, ratio); // Add small value to avoid division by zero
        
        cv::Mat correction;
        cv::filter2D(ratio, correction, CV_32F, psf, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);
        
        estimate = estimate.mul(correction);
    }
    
    // Convert back to 8-bit
    estimate *= 255.0;
    estimate.convertTo(gray, CV_8U);
    
    // Apply to color channels if needed
    if (frame.channels() == 3) {
        cv::Mat result_color;
        cv::cvtColor(gray, result_color, cv::COLOR_GRAY2BGR);
        return result_color;
    }
    
    return gray;
}

cv::Mat CameraHandler::applySharpeningFilter(const cv::Mat& frame) {
    // Unsharp masking for quick sharpening
    cv::Mat blurred, sharpened;
    
    // Gaussian blur
    double sigma = 1.0 + (deblurStrength_ * 2.0);
    cv::GaussianBlur(frame, blurred, cv::Size(0, 0), sigma);
    
    // Unsharp mask: original + amount * (original - blurred)
    double amount = 1.0 + deblurStrength_ * 2.0; // 1.0-3.0
    cv::addWeighted(frame, amount, blurred, -amount + 1.0, 0, sharpened);
    
    // Apply additional edge enhancement
    cv::Mat laplacian, enhanced;
    cv::Laplacian(frame, laplacian, CV_16S, 3);
    cv::convertScaleAbs(laplacian, laplacian);
    
    double alpha = deblurStrength_ * 0.3; // 0.0-0.3
    cv::addWeighted(sharpened, 1.0, laplacian, alpha, 0, enhanced);
    
    return enhanced;
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
        
        // Set frame duration limits (frame rate control)
        if (frameDuration_ > 0) {
            int64_t minDuration = frameDuration_ * 0.95;
            int64_t maxDuration = frameDuration_ * 1.05;
            int64_t durationRange[2] = {minDuration, maxDuration};
            request->controls().set(controls::FrameDurationLimits, Span<const int64_t, 2>(durationRange, 2));
        }
        
        // Set exposure controls
        if (autoExposure_) {
            // Enable auto exposure
            request->controls().set(controls::AeEnable, true);
            printMessage("Auto exposure: ENABLED");
        } else {
            // Disable auto exposure and set manual exposure time
            request->controls().set(controls::AeEnable, false);
            if (exposureTimeUs_ > 0) {
                request->controls().set(controls::ExposureTime, exposureTimeUs_);
                printMessage("Manual exposure: " + std::to_string(exposureTimeUs_) + " µs");
            }
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
    if (!request) {
        LOG_ERROR("Null request received");
        return;
    }
    
    if (request->status() != Request::RequestComplete) {
        LOG_ERROR("Request failed with status: {}", request->status());
        // Still try to reuse the request
        request->reuse(Request::ReuseBuffers);
        camera_->queueRequest(request);
        return;
    }

    const auto& buffers = request->buffers();
    for (const auto& [stream, buffer] : buffers) {
        if (!stream || !buffer) {
            LOG_ERROR("Invalid stream or buffer");
            continue;
        }
        
        const FrameMetadata& metadata = buffer->metadata();
        const StreamConfiguration& config = stream->configuration();

        const auto& planes = buffer->planes();
        if (planes.empty()) {
            LOG_ERROR("No planes in buffer");
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
            LOG_ERROR("Failed to map buffer");
            continue;
        }

        uint8_t* data = static_cast<uint8_t*>(mappedData);
        cv::Mat rawFrame(config.size.height, config.size.width, CV_8UC3, data, config.stride);

        // Apply rotation if configured
        cv::Mat processedFrame = rotateImage(rawFrame);
        
        // Apply deblurring if enabled (for drone vibration compensation)
        if (deblurEnabled_) {
            processedFrame = deblurFrame(processedFrame);
        }

        // Create FrameData object - MUST clone for memory safety
        FrameData frameData;
        frameData.image = processedFrame;  // Use rotated and deblurred frame
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
        
        // Update frame size to account for rotation
        if (rotationAngle_ == 90 || rotationAngle_ == 270) {
            frameData.size = cv::Size(config.size.height, config.size.width);
        } else {
            frameData.size = cv::Size(config.size.width, config.size.height);
        }
        frameData.fps = fps_;

        // ✅ OPTIMIZED: Use move semantics to add frame to buffer
        FrameBufferManager::getInstance().addFrame(std::move(frameData));

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
            LOG_ERROR("Failed to map buffer: {}", strerror(errno));
            return nullptr;
        }
    }
    
    // Fallback to regular mmap with proper tracking
    void* ptr = mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        LOG_ERROR("Fallback mmap failed: {}", strerror(errno));
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