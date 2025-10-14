#ifndef COLOR_DETECTOR_H
#define COLOR_DETECTOR_H

#include "../model.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

/**
 * @brief Color detection based on HSV color space
 * Detects objects by color range and provides bounding boxes for tracking
 */
class ColorDetector {
public:
    struct ColorRange {
        cv::Scalar lowerBound;  // Lower HSV bound (H: 0-179, S: 0-255, V: 0-255)
        cv::Scalar upperBound;  // Upper HSV bound
        std::string name;       // Color name (e.g., "red", "blue")
    };

    struct Config {
        std::vector<ColorRange> colorRanges;
        int minArea = 500;           // Minimum contour area to detect
        int maxArea = 500000;        // Maximum contour area to detect
        bool enableMorphology = true; // Apply morphological operations
        int morphKernelSize = 5;     // Kernel size for morphological operations
    };

    explicit ColorDetector(const Config& config);
    ~ColorDetector() = default;

    /**
     * @brief Detect colored objects in the frame
     * @param frame Input frame (BGR format)
     * @return Vector of detections with bounding boxes, confidence, and class info
     */
    std::vector<model::Detection> detect(const cv::Mat& frame);

    /**
     * @brief Get the target class ID (always 0 for color detection)
     */
    int getTargetClassId() const { return 0; }

private:
    Config m_config;
    cv::Mat m_hsvFrame;
    cv::Mat m_mask;
    cv::Mat m_morphKernel;

    /**
     * @brief Process mask with morphological operations
     */
    void applyMorphology(cv::Mat& mask);

    /**
     * @brief Find contours and create detections from mask
     */
    std::vector<model::Detection> createDetectionsFromMask(
        const cv::Mat& mask, 
        const std::string& colorName,
        int classId
    );
};

#endif // COLOR_DETECTOR_H
