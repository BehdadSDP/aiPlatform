#pragma once

#include "../detector_interface.h"
#include "../model.h"
#include <memory>
#include <string>
#include <vector>

/**
 * @brief YOLO detector implementation using the DetectorInterface
 * 
 * This class wraps the existing 'model' class (YOLO detector) to implement
 * the common DetectorInterface, enabling factory pattern usage.
 */
class YOLODetector : public DetectorInterface {
public:
    /**
     * @brief Constructor
     * @param modelPath Path to ONNX model file
     * @param namesPath Path to class names file
     * @param targetClassId Target class ID for filtering (-1 for all classes)
     */
    YOLODetector(const std::string& modelPath, 
                 const std::string& namesPath, 
                 int targetClassId = -1);
    
    ~YOLODetector() override = default;
    
    // DetectorInterface implementation
    std::vector<model::Detection> detect(const cv::Mat& frame) override;
    std::string getName() const override;
    bool isInitialized() const override;
    std::vector<std::string> getClassNames() const override;
    
private:
    std::unique_ptr<model> yoloModel_;
    std::vector<std::string> classNames_;
    bool initialized_;
    std::string modelType_;  // "YOLO-COCO", "YOLO-Helmet", "YOLO-Face", etc.
    
    // Helper to load class names from file
    bool loadClassNames(const std::string& namesPath);
};
