#ifndef TRAFFIC_INTENSITY_MANAGER_H
#define TRAFFIC_INTENSITY_MANAGER_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <set>
#include <algorithm>
#include "include/model.h"

struct TrafficPolygon {
    std::vector<cv::Point> points;
    std::string name;
    bool isActive;
    cv::Scalar color;
    
    TrafficPolygon(const std::vector<cv::Point>& polygonPoints, const std::string& polygonName, bool active = true) 
        : points(polygonPoints), name(polygonName), isActive(active), color(cv::Scalar(255, 0, 255)) {}  // Magenta color
};

struct VehicleCount {
    int vehicles = 0;  // Current vehicles in polygon
    
    void reset() {
        vehicles = 0;
    }
    
    void setCurrentCount(int count) {
        vehicles = count;
    }
};

struct TrafficOccupancyData {
    VehicleCount currentCount;  // Current vehicles in polygon
    
    TrafficOccupancyData() {}
};

class TrafficIntensityManager {
public:
    TrafficIntensityManager();
    
    // Traffic polygon management
    void addTrafficPolygon(const std::vector<cv::Point>& points, const std::string& name);
    void removeTrafficPolygon(const std::string& name);
    void clearAllPolygons();
    void setPolygonActive(const std::string& name, bool active);
    
    // Load traffic polygons from configuration
    void loadTrafficPolygonsFromConfig(const std::map<std::string, std::string>& config);
    
    // Vehicle counting
    void processVehicleDetections(const std::vector<model::Detection>& detections,
                                const std::vector<std::string>& classNames);
    
    // Current occupancy
    VehicleCount getCurrentCount(const std::string& polygonName = "") const;
    
    // Visualization
    void drawTrafficPolygons(cv::Mat& frame);
    void drawOccupancyStats(cv::Mat& frame);
    
    // Reset and control
    void resetCounts(const std::string& polygonName = "");
    void setEnabled(bool enabled) { enabled_ = enabled; }
    
    // Utilities
    bool isEnabled() const { return enabled_; }
    size_t getPolygonCount() const { return trafficPolygons_.size(); }
    bool hasActivePolygons() const;
    std::vector<std::string> getPolygonNames() const;
    
    // Check if a point is inside any active traffic polygon
    bool isPointInActivePolygon(const cv::Point& point) const;
    
    // Vehicle class checking (simplified for single vehicle class)
    static bool isVehicleClass(const std::string& className);

private:
    bool enabled_;
    std::vector<TrafficPolygon> trafficPolygons_;
    std::map<std::string, TrafficOccupancyData> occupancyData_;
    
    // Configuration
    bool showPolygons_;
    bool showStats_;
    
    // Helper methods
    bool isVehicleInPolygon(const cv::Point& vehicleCenter, const std::vector<cv::Point>& polygon) const;
    std::vector<cv::Point> parsePolygonFromString(const std::string& polygonStr);
    std::string formatVehicleCount(const VehicleCount& count);
};

#endif // TRAFFIC_INTENSITY_MANAGER_H 