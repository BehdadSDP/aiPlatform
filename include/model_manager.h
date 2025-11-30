#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include "detector_interface.h"
#include "model.h"
#include <memory>
#include <string>
#include <vector>

enum class ModelType {
    COCO_GENERAL = 0,
    HELMET_DETECTION = 1,
    FACE_DETECTION = 2,
    COLOR_DETECTION = 3,
    APRILTAG_DETECTION = 4
};

struct ModelConfig {
    ModelType type;
    std::string modelPath;
    std::string classNamesPath;
    std::vector<std::string> classNames;
    int targetClassId;
    
    // Color detection specific configuration
    struct ColorDetectionConfig {
        std::vector<std::string> targetColors;  // e.g., {"red", "blue", "green"}
        double minContourArea = 500.0;
        double maxContourArea = 50000.0;
    } colorConfig;
    
    // AprilTag detection specific configuration
    struct AprilTagConfig {
        int tagFamily = 2;                      // 0=16h5, 1=25h9, 2=36h11 (default)
        std::vector<int> targetTagIds;          // Empty = detect all tags
        int minMarkerPerimeter = 50;            // Minimum tag size (pixels)
        int maxMarkerPerimeter = 4000;          // Maximum tag size (pixels)
        bool refineDetection = true;            // Refine corner positions
    } apriltagConfig;
};

class ModelManager {
public:
    ModelManager();
    ~ModelManager();

    /**
     * @brief Factory method to create detector instances
     * @param config Configuration for the detector
     * @return Unique pointer to DetectorInterface implementation
     */
    static std::unique_ptr<DetectorInterface> createDetector(const ModelConfig& config);

    // Initialize with configuration
    bool initialize(const ModelConfig& config);
    
    // Detection method that delegates to the active detector
    std::vector<model::Detection> detect(const cv::Mat& frame);
    
    // Get current model information
    ModelType getCurrentModelType() const { return currentConfig_.type; }
    const std::vector<std::string>& getClassNames() const;
    int getTargetClassId() const { return currentConfig_.targetClassId; }
    std::string getModelName() const;
    
    // Model-specific methods
    bool isHelmetModel() const { return currentConfig_.type == ModelType::HELMET_DETECTION; }
    
    // Get detection result interpretation
    std::string getDetectionDescription(const model::Detection& detection) const;

private:
    std::unique_ptr<DetectorInterface> detector_;  // Single detector pointer (polymorphic)
    ModelConfig currentConfig_;
    bool initialized_;
    
    // Helper methods
    int getHelmetTargetClassId() const; // Returns appropriate class ID for helmet detection
};

#endif // MODEL_MANAGER_H 
