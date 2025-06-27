#pragma once

#include "include/hazard_zone_manager.h"
#include "include/traffic_intensity_manager.h"
#include "include/model.h"
#include <opencv2/opencv.hpp>
#include <map>
#include <vector>
#include <string>

class SafetyManager {
public:
    SafetyManager() = default;
    
    // Configuration loading
    void loadHazardZones(const std::map<std::string, std::string>& config);
    void loadTrafficIntensity(const std::map<std::string, std::string>& config);
    
    // Safety monitoring methods
    void processDetections(const std::vector<model::Detection>& detections, 
                          const std::vector<std::string>& classNames);
    
    // Visualization methods
    void drawSafetyOverlays(cv::Mat& frame);
    
    // Status methods
    bool isHazardZonesEnabled() const { return hazardZoneManager_.isEnabled(); }
    bool isTrafficIntensityEnabled() const { return trafficIntensityManager_.isEnabled(); }
    
    // Get managers for direct access if needed
    HazardZoneManager& getHazardZoneManager() { return hazardZoneManager_; }
    TrafficIntensityManager& getTrafficIntensityManager() { return trafficIntensityManager_; }

private:
    HazardZoneManager hazardZoneManager_;
    TrafficIntensityManager trafficIntensityManager_;
}; 