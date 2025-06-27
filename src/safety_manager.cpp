#include "include/safety_manager.h"
#include <iostream>

void SafetyManager::loadHazardZones(const std::map<std::string, std::string>& config) {
    hazardZoneManager_.loadZonesFromConfig(config);
    std::cout << "✓ Hazard zones loaded for safety monitoring" << std::endl;
}

void SafetyManager::loadTrafficIntensity(const std::map<std::string, std::string>& config) {
    trafficIntensityManager_.loadTrafficPolygonsFromConfig(config);
    std::cout << "✓ Traffic intensity loaded for safety monitoring" << std::endl;
}

void SafetyManager::processDetections(const std::vector<model::Detection>& detections, 
                                     const std::vector<std::string>& classNames) {
    // Process hazard zone violations
    if (hazardZoneManager_.isEnabled()) {
        std::vector<HazardAlert> hazardAlerts = hazardZoneManager_.checkViolations(detections, classNames);
        if (!hazardAlerts.empty()) {
            hazardZoneManager_.triggerAlarm(hazardAlerts);
            std::cout << "🚨 Safety Alert - Hazard zone violation detected!" << std::endl;
        }
    }
    
    // Process traffic intensity
    if (trafficIntensityManager_.isEnabled()) {
        trafficIntensityManager_.processVehicleDetections(detections, classNames);
    }
}

void SafetyManager::drawSafetyOverlays(cv::Mat& frame) {
    // Draw hazard zones
    if (hazardZoneManager_.isEnabled()) {
        hazardZoneManager_.drawZones(frame);
    }
    
    // Draw traffic intensity polygons and statistics
    if (trafficIntensityManager_.isEnabled()) {
        trafficIntensityManager_.drawTrafficPolygons(frame);
        trafficIntensityManager_.drawOccupancyStats(frame);
    }
} 