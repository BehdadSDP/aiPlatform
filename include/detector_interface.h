#pragma once

#include "model.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

/**
 * @brief Common interface for all object detectors
 * 
 * This interface defines the contract that all detector implementations must follow,
 * enabling polymorphic usage and the Factory Pattern for detector creation.
 * 
 * Similar to TrackerInterface, this allows the system to work with different
 * detector types (YOLO, Color, HOG, etc.) through a common interface.
 */
class DetectorInterface {
public:
    virtual ~DetectorInterface() = default;
    
    /**
     * @brief Detect objects in the given frame
     * @param frame Input frame (BGR format)
     * @return Vector of detections with bounding boxes, confidence, and class info
     */
    virtual std::vector<model::Detection> detect(const cv::Mat& frame) = 0;
    
    /**
     * @brief Get the name of the detector
     * @return Detector name (e.g., "YOLO", "Color", "HOG")
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Check if the detector is properly initialized
     * @return True if initialized and ready to use
     */
    virtual bool isInitialized() const = 0;
    
    /**
     * @brief Get the list of class names this detector can identify
     * @return Vector of class name strings
     */
    virtual std::vector<std::string> getClassNames() const = 0;
};
