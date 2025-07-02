#include "include/monitoring/traffic_intensity_manager.h"
#include "include/config_utils.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>

TrafficIntensityManager::TrafficIntensityManager() 
    : enabled_(false), showPolygons_(true), showStats_(true) {
}

void TrafficIntensityManager::addTrafficPolygon(const std::vector<cv::Point>& points, const std::string& name) {
    // Remove existing polygon with same name
    removeTrafficPolygon(name);
    
    // Add new polygon
    trafficPolygons_.emplace_back(points, name, true);
    occupancyData_[name] = TrafficOccupancyData();
    
    std::cout << "Traffic polygon added: " << name << " with " << points.size() << " points" << std::endl;
}

void TrafficIntensityManager::removeTrafficPolygon(const std::string& name) {
    auto it = std::remove_if(trafficPolygons_.begin(), trafficPolygons_.end(),
                            [&name](const TrafficPolygon& polygon) { return polygon.name == name; });
    trafficPolygons_.erase(it, trafficPolygons_.end());
    occupancyData_.erase(name);
}

void TrafficIntensityManager::clearAllPolygons() {
    trafficPolygons_.clear();
    occupancyData_.clear();
}

void TrafficIntensityManager::setPolygonActive(const std::string& name, bool active) {
    for (auto& polygon : trafficPolygons_) {
        if (polygon.name == name) {
            polygon.isActive = active;
            break;
        }
    }
}

void TrafficIntensityManager::loadTrafficPolygonsFromConfig(const std::map<std::string, std::string>& config) {
    // Check if traffic intensity is enabled
    enabled_ = config_utils::getConfigInt(config, "traffic_intensity.enabled") == 1;
    if (!enabled_) {
        std::cout << "Traffic Intensity feature is disabled" << std::endl;
        return;
    }
    
    // Load global settings
    showPolygons_ = config_utils::getConfigInt(config, "traffic_intensity.show_polygons") == 1;
    showStats_ = config_utils::getConfigInt(config, "traffic_intensity.show_stats") == 1;
    
    // Load traffic polygons
    for (int i = 1; i <= 10; ++i) {
        std::string polygonKey = "traffic_intensity.polygon" + std::to_string(i);
        std::string nameKey = polygonKey + "_name";
        std::string activeKey = polygonKey + "_active";
        
        if (config.find(polygonKey) != config.end()) {
            std::string polygonStr = config.at(polygonKey);
            std::string polygonName = config.count(nameKey) ? config.at(nameKey) : ("Traffic Polygon " + std::to_string(i));
            bool isActive = config.count(activeKey) ? (config.at(activeKey) == "1") : true;
            
            std::vector<cv::Point> polygonPoints = parsePolygonFromString(polygonStr);
            if (polygonPoints.size() >= 3) {  // Need at least 3 points for a polygon
                addTrafficPolygon(polygonPoints, polygonName);
                setPolygonActive(polygonName, isActive);
            }
        }
    }
    
    std::cout << "Traffic Intensity Manager initialized with " << trafficPolygons_.size() << " polygons" << std::endl;
}

void TrafficIntensityManager::processVehicleDetections(const std::vector<model::Detection>& detections,
                                                     const std::vector<std::string>& classNames) {
    if (!enabled_ || trafficPolygons_.empty()) return;
    
    // Reset all polygon counts to 0 for this frame
    for (auto& [polygonName, data] : occupancyData_) {
        data.currentCount.reset();
    }
    
    // Count vehicles currently in each polygon
    for (const auto& detection : detections) {
        // Get class name for this detection
        std::string className = "unknown";
        if (detection.classId >= 0 && detection.classId < static_cast<int>(classNames.size())) {
            className = classNames[detection.classId];
        }
        
        if (!isVehicleClass(className)) continue;
        
        // Get vehicle center point
        cv::Point vehicleCenter(detection.box.x + detection.box.width/2,
                               detection.box.y + detection.box.height/2);
        
        // Check if vehicle is in any traffic polygon
        for (auto& polygon : trafficPolygons_) {
            if (!polygon.isActive) continue;
            
            if (isVehicleInPolygon(vehicleCenter, polygon.points)) {
                // Get or create occupancy data for this polygon
                auto& data = occupancyData_[polygon.name];
                
                // Increment current count for this polygon
                data.currentCount.vehicles++;
            }
        }
    }
}

VehicleCount TrafficIntensityManager::getCurrentCount(const std::string& polygonName) const {
    if (polygonName.empty() && !occupancyData_.empty()) {
        // Return total count of all polygons
        VehicleCount totalCount;
        for (const auto& [name, data] : occupancyData_) {
            totalCount.vehicles += data.currentCount.vehicles;
        }
        return totalCount;
    }
    
    auto it = occupancyData_.find(polygonName);
    return (it != occupancyData_.end()) ? it->second.currentCount : VehicleCount();
}

void TrafficIntensityManager::drawTrafficPolygons(cv::Mat& frame) {
    if (!enabled_ || !showPolygons_) return;
    
    for (const auto& polygon : trafficPolygons_) {
        cv::Scalar color = polygon.isActive ? polygon.color : cv::Scalar(128, 128, 128);
        
        // Draw polygon outline
        if (polygon.points.size() >= 3) {
            std::vector<std::vector<cv::Point>> polygons = {polygon.points};
            cv::polylines(frame, polygons, true, color, 1);
            
            // Optional: Fill polygon with semi-transparent color
            cv::Mat overlay = frame.clone();
            cv::fillPoly(overlay, polygons, color);
            cv::addWeighted(frame, 0.8, overlay, 0.2, 0, frame);
        }
        
        // Draw polygon label at the center
        if (!polygon.points.empty()) {
            // Calculate polygon center (centroid)
            cv::Point center(0, 0);
            for (const auto& point : polygon.points) {
                center.x += point.x;
                center.y += point.y;
            }
            center.x /= polygon.points.size();
            center.y /= polygon.points.size();
            
            std::string label = polygon.name + (polygon.isActive ? " [ACTIVE]" : " [INACTIVE]");
            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, nullptr);
            cv::Point textPos(center.x - textSize.width/2, center.y);
            
            // Background for text
            cv::rectangle(frame, 
                         cv::Point(textPos.x - 2, textPos.y - textSize.height - 2),
                         cv::Point(textPos.x + textSize.width + 2, textPos.y + 2),
                         color, -1);
            
            cv::putText(frame, label, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.5, 
                       cv::Scalar(255, 255, 255), 1);
        }
    }
}

void TrafficIntensityManager::drawOccupancyStats(cv::Mat& frame) {
    if (!enabled_ || !showStats_) return;
    
    int yOffset = 30;
    for (const auto& [polygonName, data] : occupancyData_) {
        std::string statsText = polygonName + ": " + formatVehicleCount(data.currentCount);
        
        // Green for 0 vehicles, yellow for 1-3, red for 4+
        cv::Scalar textColor = (data.currentCount.vehicles == 0) ? cv::Scalar(0, 255, 0) :
                              (data.currentCount.vehicles <= 3) ? cv::Scalar(0, 255, 255) :
                              cv::Scalar(0, 0, 255);
        
        cv::putText(frame, statsText, cv::Point(10, yOffset), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, textColor, 2);
        yOffset += 25;
    }
}

void TrafficIntensityManager::resetCounts(const std::string& polygonName) {
    if (polygonName.empty()) {
        // Reset all polygons
        for (auto& [name, data] : occupancyData_) {
            data.currentCount.reset();
        }
    } else {
        auto it = occupancyData_.find(polygonName);
        if (it != occupancyData_.end()) {
            it->second.currentCount.reset();
        }
    }
}

bool TrafficIntensityManager::hasActivePolygons() const {
    return std::any_of(trafficPolygons_.begin(), trafficPolygons_.end(),
                      [](const TrafficPolygon& polygon) { return polygon.isActive; });
}

std::vector<std::string> TrafficIntensityManager::getPolygonNames() const {
    std::vector<std::string> names;
    for (const auto& polygon : trafficPolygons_) {
        names.push_back(polygon.name);
    }
    return names;
}

bool TrafficIntensityManager::isPointInActivePolygon(const cv::Point& point) const {
    if (!enabled_) return false;
    
    for (const auto& polygon : trafficPolygons_) {
        if (polygon.isActive && polygon.points.size() >= 3) {
            if (isVehicleInPolygon(point, polygon.points)) {
                return true;
            }
        }
    }
    return false;
}

bool TrafficIntensityManager::isVehicleClass(const std::string& className) {
    // Convert to lowercase for case-insensitive matching
    std::string lowerClassName = className;
    std::transform(lowerClassName.begin(), lowerClassName.end(), lowerClassName.begin(), ::tolower);
    
    // Only check for "vehicle" class
    return (lowerClassName == "vehicle" || lowerClassName == "vehicles");
}

// Private helper methods
bool TrafficIntensityManager::isVehicleInPolygon(const cv::Point& vehicleCenter, const std::vector<cv::Point>& polygon) const {
    // Use OpenCV's pointPolygonTest function
    return cv::pointPolygonTest(polygon, vehicleCenter, false) >= 0;
}

std::vector<cv::Point> TrafficIntensityManager::parsePolygonFromString(const std::string& polygonStr) {
    std::istringstream iss(polygonStr);
    std::string token;
    std::vector<int> values;
    
    // Parse comma-separated values
    while (std::getline(iss, token, ',')) {
        try {
            values.push_back(std::stoi(token));
        } catch (const std::exception&) {
            return {};  // Return empty vector on error
        }
    }
    
    // Convert to points (need pairs of x,y coordinates)
    std::vector<cv::Point> points;
    for (size_t i = 0; i + 1 < values.size(); i += 2) {
        points.emplace_back(values[i], values[i + 1]);
    }
    
    return points;
}

std::string TrafficIntensityManager::formatVehicleCount(const VehicleCount& count) {
    return std::to_string(count.vehicles) + " vehicles";
}
