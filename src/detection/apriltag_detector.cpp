#include "include/detection/apriltag_detector.h"
#include "include/logger.h"
#include <sstream>

AprilTagDetector::AprilTagDetector(const Config& config)
    : m_config(config), m_initialized(false) {
    m_initialized = initializeDetector();
}

bool AprilTagDetector::initializeDetector() {
    try {
        // Get dictionary type based on tag family
        cv::aruco::PredefinedDictionaryType dictType = getDictionaryType();
        m_dictionary = cv::aruco::getPredefinedDictionary(dictType);

        // Configure detector parameters
        m_detectorParams.adaptiveThreshWinSizeMin = static_cast<int>(m_config.adaptiveThreshWinSizeMin);
        m_detectorParams.adaptiveThreshWinSizeMax = static_cast<int>(m_config.adaptiveThreshWinSizeMax);
        m_detectorParams.adaptiveThreshWinSizeStep = static_cast<int>(m_config.adaptiveThreshWinSizeStep);
        m_detectorParams.minMarkerPerimeterRate = m_config.minMarkerPerimeter / 1000.0;
        m_detectorParams.maxMarkerPerimeterRate = m_config.maxMarkerPerimeter / 1000.0;
        m_detectorParams.minDistanceToBorder = m_config.minDistanceToBorder;
        
        // Enable corner refinement for better accuracy
        if (m_config.refineDetection) {
            m_detectorParams.cornerRefinementMethod = cv::aruco::CORNER_REFINE_SUBPIX;
        } else {
            m_detectorParams.cornerRefinementMethod = cv::aruco::CORNER_REFINE_NONE;
        }

        // Create detector (pass dictionary and params by reference)
        m_detector = new cv::aruco::ArucoDetector(m_dictionary, m_detectorParams);

        LOG_INFO("AprilTag detector initialized:");
        LOG_INFO("  Tag Family: {}", getTagFamilyName());
        LOG_INFO("  Target Tags: {}", 
                 m_config.targetTagIds.empty() ? "All" : std::to_string(m_config.targetTagIds.size()));
        LOG_INFO("  Corner Refinement: {}", m_config.refineDetection ? "Enabled" : "Disabled");

        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize AprilTag detector: {}", e.what());
        return false;
    }
}

cv::aruco::PredefinedDictionaryType AprilTagDetector::getDictionaryType() const {
    // Map AprilTag families to OpenCV ArUco dictionaries
    // OpenCV uses ArUco but supports AprilTag families
    switch (m_config.family) {
        case TagFamily::TAG_16h5:
            return cv::aruco::DICT_APRILTAG_16h5;
        case TagFamily::TAG_25h9:
            return cv::aruco::DICT_APRILTAG_25h9;
        case TagFamily::TAG_36h11:
            return cv::aruco::DICT_APRILTAG_36h11;
        case TagFamily::TAG_STANDARD41h12:
            return cv::aruco::DICT_APRILTAG_36h11; // Fallback to 36h11
        case TagFamily::TAG_STANDARD52h13:
            return cv::aruco::DICT_APRILTAG_36h11; // Fallback to 36h11
        default:
            return cv::aruco::DICT_APRILTAG_36h11;
    }
}

std::string AprilTagDetector::getTagFamilyName() const {
    switch (m_config.family) {
        case TagFamily::TAG_16h5: return "16h5";
        case TagFamily::TAG_25h9: return "25h9";
        case TagFamily::TAG_36h11: return "36h11";
        case TagFamily::TAG_STANDARD41h12: return "Standard41h12";
        case TagFamily::TAG_STANDARD52h13: return "Standard52h13";
        default: return "Unknown";
    }
}

std::vector<model::Detection> AprilTagDetector::detect(const cv::Mat& frame) {
    std::vector<model::Detection> detections;
    m_lastDetectedIds.clear();

    if (!m_initialized || frame.empty()) {
        return detections;
    }

    try {
        // Convert to grayscale if needed
        cv::Mat gray;
        if (frame.channels() == 3) {
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = frame;
        }

        // Detect AprilTags
        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners, rejectedCandidates;
        
        m_detector->detectMarkers(gray, corners, ids, rejectedCandidates);

        // Convert to Detection format
        for (size_t i = 0; i < ids.size(); ++i) {
            int tagId = ids[i];

            // Filter by target IDs if specified
            if (!shouldDetectTag(tagId)) {
                continue;
            }

            // Calculate bounding box from corners
            const std::vector<cv::Point2f>& tagCorners = corners[i];
            
            // Find min/max coordinates
            float minX = tagCorners[0].x, maxX = tagCorners[0].x;
            float minY = tagCorners[0].y, maxY = tagCorners[0].y;
            
            for (const auto& corner : tagCorners) {
                minX = std::min(minX, corner.x);
                maxX = std::max(maxX, corner.x);
                minY = std::min(minY, corner.y);
                maxY = std::max(maxY, corner.y);
            }

            // Create bounding box
            cv::Rect bbox(
                static_cast<int>(minX),
                static_cast<int>(minY),
                static_cast<int>(maxX - minX),
                static_cast<int>(maxY - minY)
            );

            // Ensure bbox is within frame bounds
            bbox.x = std::max(0, bbox.x);
            bbox.y = std::max(0, bbox.y);
            bbox.width = std::min(frame.cols - bbox.x, bbox.width);
            bbox.height = std::min(frame.rows - bbox.y, bbox.height);

            // Create detection
            model::Detection detection;
            detection.box = bbox;
            detection.confidence = 1.0f; // AprilTags are binary (detected or not)
            detection.classId = tagId;   // Use tag ID as class ID

            detections.push_back(detection);
            m_lastDetectedIds.push_back(tagId);
        }

        LOG_DEBUG("Detected {} AprilTags", detections.size());

    } catch (const std::exception& e) {
        LOG_ERROR("AprilTag detection failed: {}", e.what());
    }

    return detections;
}

bool AprilTagDetector::shouldDetectTag(int tagId) const {
    // If no target tags specified, detect all
    if (m_config.targetTagIds.empty()) {
        return true;
    }

    // Check if tag ID is in target list
    return std::find(m_config.targetTagIds.begin(), 
                     m_config.targetTagIds.end(), 
                     tagId) != m_config.targetTagIds.end();
}

std::vector<std::string> AprilTagDetector::getClassNames() const {
    return createClassNames();
}

std::vector<std::string> AprilTagDetector::createClassNames() const {
    std::vector<std::string> classNames;

    if (m_config.targetTagIds.empty()) {
        // Generic name for all tags
        classNames.push_back("AprilTag");
    } else {
        // Create specific names for target tags
        for (int tagId : m_config.targetTagIds) {
            std::ostringstream oss;
            oss << "Tag_" << tagId;
            classNames.push_back(oss.str());
        }
    }

    return classNames;
}
