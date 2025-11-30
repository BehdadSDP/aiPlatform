#ifndef APRILTAG_DETECTOR_H
#define APRILTAG_DETECTOR_H

#include "../detector_interface.h"
#include "../model.h"
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>
#include <vector>
#include <string>
#include <map>

/**
 * @brief AprilTag detection using OpenCV's AprilTag detector
 * Detects fiducial markers and provides bounding boxes for tracking
 * Implements DetectorInterface for factory pattern usage
 * 
 * AprilTags are useful for:
 * - Robot localization and navigation
 * - AR/VR applications
 * - Precise object tracking
 * - Camera calibration
 */
class AprilTagDetector : public DetectorInterface {
public:
    enum class TagFamily {
        TAG_16h5,      // 16h5 family (30 unique tags)
        TAG_25h9,      // 25h9 family (35 unique tags)
        TAG_36h11,     // 36h11 family (587 unique tags, most common)
        TAG_STANDARD41h12,  // Standard41h12 family
        TAG_STANDARD52h13   // Standard52h13 family
    };

    struct Config {
        TagFamily family = TagFamily::TAG_36h11;  // Default to most common
        std::vector<int> targetTagIds;             // Empty = detect all tags
        int minMarkerPerimeter = 50;               // Minimum tag size (pixels)
        int maxMarkerPerimeter = 4000;             // Maximum tag size (pixels)
        bool refineDetection = true;               // Refine corner positions
        float adaptiveThreshWinSizeMin = 3.0f;    // Adaptive threshold window size
        float adaptiveThreshWinSizeMax = 23.0f;
        float adaptiveThreshWinSizeStep = 10.0f;
        int minDistanceToBorder = 3;               // Minimum distance to image border
    };

    explicit AprilTagDetector(const Config& config);
    ~AprilTagDetector() override = default;

    // DetectorInterface implementation
    std::vector<model::Detection> detect(const cv::Mat& frame) override;
    std::string getName() const override { return "AprilTag Detection"; }
    bool isInitialized() const override { return m_initialized; }
    std::vector<std::string> getClassNames() const override;

    /**
     * @brief Get the target class ID (always 0 for AprilTag detection)
     */
    int getTargetClassId() const { return 0; }

    /**
     * @brief Get detected tag IDs from last detection
     */
    std::vector<int> getLastDetectedTagIds() const { return m_lastDetectedIds; }

    /**
     * @brief Get tag family name
     */
    std::string getTagFamilyName() const;

private:
    Config m_config;
    cv::Ptr<cv::aruco::ArucoDetector> m_detector;
    cv::aruco::DetectorParameters m_detectorParams;
    cv::aruco::Dictionary m_dictionary;
    bool m_initialized;
    std::vector<int> m_lastDetectedIds;

    /**
     * @brief Initialize AprilTag detector with specified family
     */
    bool initializeDetector();

    /**
     * @brief Convert tag family enum to OpenCV dictionary
     */
    cv::aruco::PredefinedDictionaryType getDictionaryType() const;

    /**
     * @brief Create class names list for detected tags
     */
    std::vector<std::string> createClassNames() const;

    /**
     * @brief Check if tag ID should be detected (based on targetTagIds filter)
     */
    bool shouldDetectTag(int tagId) const;
};

#endif // APRILTAG_DETECTOR_H
