#include "include/detection/color_detector.h"
#include <iostream>

ColorDetector::ColorDetector(const Config& config)
    : m_config(config)
{
    if (m_config.enableMorphology) {
        m_morphKernel = cv::getStructuringElement(
            cv::MORPH_ELLIPSE,
            cv::Size(m_config.morphKernelSize, m_config.morphKernelSize)
        );
    }

    std::cout << "ColorDetector initialized with " 
              << m_config.colorRanges.size() << " color ranges" << std::endl;
    for (const auto& range : m_config.colorRanges) {
        std::cout << "  - Color: " << range.name 
                  << " HSV Range: [" << range.lowerBound[0] << "," 
                  << range.lowerBound[1] << "," << range.lowerBound[2] 
                  << "] to [" << range.upperBound[0] << "," 
                  << range.upperBound[1] << "," << range.upperBound[2] << "]"
                  << std::endl;
    }
}

std::vector<model::Detection> ColorDetector::detect(const cv::Mat& frame) {
    if (frame.empty()) {
        return {};
    }

    std::vector<model::Detection> allDetections;

    // Convert BGR to HSV
    cv::cvtColor(frame, m_hsvFrame, cv::COLOR_BGR2HSV);

    // Process each color range
    for (size_t i = 0; i < m_config.colorRanges.size(); ++i) {
        const auto& colorRange = m_config.colorRanges[i];

        // Create mask for this color range
        cv::inRange(m_hsvFrame, colorRange.lowerBound, colorRange.upperBound, m_mask);

        // Apply morphological operations to reduce noise
        if (m_config.enableMorphology) {
            applyMorphology(m_mask);
        }

        // Find detections from this mask
        auto detections = createDetectionsFromMask(m_mask, colorRange.name, static_cast<int>(i));
        allDetections.insert(allDetections.end(), detections.begin(), detections.end());
    }

    return allDetections;
}

void ColorDetector::applyMorphology(cv::Mat& mask) {
    // Remove small noise with opening (erosion followed by dilation)
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, m_morphKernel);
    
    // Fill small holes with closing (dilation followed by erosion)
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, m_morphKernel);
}

std::vector<model::Detection> ColorDetector::createDetectionsFromMask(
    const cv::Mat& mask,
    const std::string& colorName,
    int classId
) {
    std::vector<model::Detection> detections;

    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Process each contour
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);

        // Filter by area
        if (area < m_config.minArea || area > m_config.maxArea) {
            continue;
        }

        // Get bounding box
        cv::Rect bbox = cv::boundingRect(contour);

        // Create detection
        model::Detection detection;
        //bbox.width = (int)(bbox.width * 1.5);
        //bbox.height = (int)(bbox.height * 1.5);
        //bbox.x = (int)(bbox.x * 0.5);
        //bbox.y = (int)(bbox.y * 0.5);
        //cv::Rect bbox_(bbox.tl() * 2, bbox.br() * 2);

        detection.box = bbox;
        detection.classId = classId;
        
        // Calculate confidence based on how "filled" the bounding box is
        // A more filled box indicates a better detection
        double fillRatio = area / (bbox.width * bbox.height);
        detection.confidence = static_cast<float>(std::min(fillRatio * 1.2, 1.0));

        detections.push_back(detection);
    }

    return detections;
}

std::vector<std::string> ColorDetector::getClassNames() const {
    std::vector<std::string> names;
    for (const auto& range : m_config.colorRanges) {
        names.push_back(range.name);
    }
    return names;
}
