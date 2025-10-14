#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include "model.h"
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class ColorDetector;

enum class ModelType {
    COCO_GENERAL = 0,
    HELMET_DETECTION = 1,
    FACE_DETECTION = 2,
    COLOR_DETECTION = 3
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
};

class ModelManager {
public:
    ModelManager();
    ~ModelManager();

    // Initialize with configuration
    bool initialize(const ModelConfig& config);
    
    // Detection method that delegates to the active model
    std::vector<model::Detection> detect(const cv::Mat& frame);
    
    // Get current model information
    ModelType getCurrentModelType() const { return currentConfig_.type; }
    const std::vector<std::string>& getClassNames() const { return currentConfig_.classNames; }
    int getTargetClassId() const { return currentConfig_.targetClassId; }
    
    // Model-specific methods
    bool isHelmetModel() const { return currentConfig_.type == ModelType::HELMET_DETECTION; }
    
    // Get detection result interpretation
    std::string getDetectionDescription(const model::Detection& detection) const;

private:
    std::unique_ptr<model> activeModel_;
    std::unique_ptr<ColorDetector> colorDetector_;
    ModelConfig currentConfig_;
    bool initialized_;
    
    // Helper methods
    bool loadClassNames(const std::string& classNamesPath, std::vector<std::string>& classNames);
    int getHelmetTargetClassId() const; // Returns appropriate class ID for helmet detection
    bool initializeColorDetection(const ModelConfig& config);
};

#endif // MODEL_MANAGER_H 
