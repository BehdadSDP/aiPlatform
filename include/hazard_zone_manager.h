#ifndef HAZARD_ZONE_MANAGER_H
#define HAZARD_ZONE_MANAGER_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <chrono>
#include "include/model.h"

struct HazardZone {
    cv::Rect area;
    std::string name;
    bool isActive;
    cv::Scalar color;
    
    HazardZone(const cv::Rect& rect, const std::string& zoneName, bool active = true) 
        : area(rect), name(zoneName), isActive(active), color(cv::Scalar(0, 0, 255)) {}
};

struct HazardAlert {
    std::string zoneName;
    cv::Rect personBox;
    cv::Rect zoneArea;
    float confidence;
    std::chrono::steady_clock::time_point alertTime;
    
    HazardAlert(const std::string& zone, const cv::Rect& person, const cv::Rect& area, float conf)
        : zoneName(zone), personBox(person), zoneArea(area), confidence(conf), 
          alertTime(std::chrono::steady_clock::now()) {}
};

class HazardZoneManager {
public:
    HazardZoneManager();
    
    // Zone management
    void addZone(const cv::Rect& area, const std::string& name);
    void removeZone(const std::string& name);
    void clearAllZones();
    void setZoneActive(const std::string& name, bool active);
    
    // Load zones from configuration
    void loadZonesFromConfig(const std::map<std::string, std::string>& config);
    
    // Detection monitoring
    std::vector<HazardAlert> checkViolations(const std::vector<model::Detection>& detections,
                                           const std::vector<std::string>& classNames);
    
    // Visualization
    void drawZones(cv::Mat& frame);
    void drawAlerts(cv::Mat& frame, const std::vector<HazardAlert>& alerts);
    void drawZoneStatus(cv::Mat& frame);
    
    // Alarm system
    void triggerAlarm(const std::vector<HazardAlert>& alerts);
    void setAlarmEnabled(bool enabled) { alarmEnabled_ = enabled; }
    void setVisualAlarmEnabled(bool enabled) { visualAlarmEnabled_ = enabled; }
    void setAudioAlarmEnabled(bool enabled) { audioAlarmEnabled_ = enabled; }
    
    // Visualization control
    void setShowZones(bool show) { showZones_ = show; }
    void setShowZoneStatus(bool show) { showZoneStatus_ = show; }
    bool getShowZones() const { return showZones_; }
    bool getShowZoneStatus() const { return showZoneStatus_; }
    
    // Utilities
    size_t getZoneCount() const { return zones_.size(); }
    bool hasActiveZones() const;
    bool isEnabled() const { return enabled_; }
    
private:
    bool enabled_;
    std::vector<HazardZone> zones_;
    bool alarmEnabled_;
    bool visualAlarmEnabled_;
    bool audioAlarmEnabled_;
    
    // Visualization settings
    bool showZones_;
    bool showZoneStatus_;
    
    // Alarm state
    std::chrono::steady_clock::time_point lastAlarmTime_;
    int alarmCooldownMs_;
    bool currentlyAlarming_;
    
    // Helper methods
    bool isPersonInZone(const cv::Rect& personBox, const cv::Rect& zoneArea);
    cv::Rect parseRectFromString(const std::string& rectStr);
    void playAlarmSound();
    void showVisualAlarm(cv::Mat& frame);
};

#endif // HAZARD_ZONE_MANAGER_H 