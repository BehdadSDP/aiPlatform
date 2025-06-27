#include "include/hazard_zone_manager.h"
#include "include/config_utils.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>

HazardZoneManager::HazardZoneManager() 
    : alarmEnabled_(true), visualAlarmEnabled_(true), audioAlarmEnabled_(false),
      showZones_(true), showZoneStatus_(true), alarmCooldownMs_(2000), currentlyAlarming_(false) {
    lastAlarmTime_ = std::chrono::steady_clock::now() - std::chrono::milliseconds(alarmCooldownMs_);
    enabled_ = true;
}

void HazardZoneManager::addZone(const cv::Rect& area, const std::string& name) {
    // Remove existing zone with same name
    removeZone(name);
    
    zones_.emplace_back(area, name);
    std::cout << "Added hazard zone '" << name << "': " << area << std::endl;
}

void HazardZoneManager::removeZone(const std::string& name) {
    zones_.erase(std::remove_if(zones_.begin(), zones_.end(),
        [&name](const HazardZone& zone) { return zone.name == name; }), zones_.end());
}

void HazardZoneManager::clearAllZones() {
    zones_.clear();
    std::cout << "All hazard zones cleared" << std::endl;
}

void HazardZoneManager::setZoneActive(const std::string& name, bool active) {
    for (auto& zone : zones_) {
        if (zone.name == name) {
            zone.isActive = active;
            std::cout << "Zone '" << name << "' " << (active ? "activated" : "deactivated") << std::endl;
            return;
        }
    }
}

void HazardZoneManager::loadZonesFromConfig(const std::map<std::string, std::string>& config) {
    clearAllZones();
    
    // Check if the entire feature is enabled first
    try {
        enabled_ = config_utils::getConfigInt(config, "hazard_zones.enabled") == 1;
    } catch (const std::exception& e) {
        enabled_ = true; // Default to enabled if not found
    }

    if (!enabled_) {
        std::cout << "\n=== HAZARD ZONE FEATURE DISABLED ===\n" << std::endl;
        alarmEnabled_ = false;
        visualAlarmEnabled_ = false;
        audioAlarmEnabled_ = false;
        return;
    }
    
    // Load alarm settings
    try {
        alarmEnabled_ = config_utils::getConfigInt(config, "hazard_zones.alarm_enabled") == 1;
        visualAlarmEnabled_ = config_utils::getConfigInt(config, "hazard_zones.visual_alarm") == 1;
        audioAlarmEnabled_ = config_utils::getConfigInt(config, "hazard_zones.audio_alarm") == 1;
        alarmCooldownMs_ = config_utils::getConfigInt(config, "hazard_zones.alarm_cooldown_ms");
    } catch (const std::exception& e) {
        std::cout << "Using default alarm settings: " << e.what() << std::endl;
    }
    
    // Load visualization settings
    try {
        showZones_ = config_utils::getConfigInt(config, "hazard_zones.show_zones") == 1;
        showZoneStatus_ = config_utils::getConfigInt(config, "hazard_zones.show_zone_status") == 1;
    } catch (const std::exception& e) {
        std::cout << "Using default visualization settings: " << e.what() << std::endl;
    }
    
    std::cout << "\n=== HAZARD ZONE CONFIGURATION ===" << std::endl;
    std::cout << "Alarm enabled: " << (alarmEnabled_ ? "YES" : "NO") << std::endl;
    std::cout << "Visual alarm: " << (visualAlarmEnabled_ ? "YES" : "NO") << std::endl;
    std::cout << "Audio alarm: " << (audioAlarmEnabled_ ? "YES" : "NO") << std::endl;
    std::cout << "Cooldown: " << alarmCooldownMs_ << "ms" << std::endl;
    std::cout << "Show zones: " << (showZones_ ? "YES" : "NO") << std::endl;
    std::cout << "Show status: " << (showZoneStatus_ ? "YES" : "NO") << std::endl;
    
    // Load zones (zone1, zone2, etc.)
    for (int i = 1; i <= 10; ++i) {
        std::string zoneKey = "hazard_zones.zone" + std::to_string(i);
        std::string nameKey = "hazard_zones.zone" + std::to_string(i) + "_name";
        std::string activeKey = "hazard_zones.zone" + std::to_string(i) + "_active";
        
        try {
            std::string zoneStr = config_utils::getConfigString(config, zoneKey);
            cv::Rect area = parseRectFromString(zoneStr);
            
            std::string name = "Zone" + std::to_string(i);
            try {
                name = config_utils::getConfigString(config, nameKey);
            } catch (...) {} // Use default name if not specified
            
            bool active = true;
            try {
                active = config_utils::getConfigInt(config, activeKey) == 1;
            } catch (...) {} // Use default active if not specified
            
            if (area.width > 0 && area.height > 0) {
                zones_.emplace_back(area, name, active);
                std::cout << "✓ Zone " << i << ": " << name << " = [" << area.x << "," << area.y 
                         << "," << area.width << "," << area.height << "] (active: " 
                         << (active ? "YES" : "NO") << ")" << std::endl;
            }
        } catch (const std::exception& e) {
            // Zone not defined, continue to next
            continue;
        }
    }
    
    std::cout << "Total loaded zones: " << zones_.size() << std::endl;
    if (zones_.empty()) {
        std::cout << "⚠️  WARNING: No hazard zones configured!" << std::endl;
        std::cout << "   Add zones to config.txt like: hazard_zones.zone1=x,y,width,height" << std::endl;
    }
    std::cout << "================================\n" << std::endl;
}

std::vector<HazardAlert> HazardZoneManager::checkViolations(const std::vector<model::Detection>& detections,
                                                           const std::vector<std::string>& classNames) {
    std::vector<HazardAlert> alerts;
    
    if (!enabled_ || !hasActiveZones()) return alerts;
    
    // Check each detection
    for (const auto& detection : detections) {
        // Only check people (class_id 0 for COCO dataset)
        if (detection.classId == 0 && detection.confidence > 0.3f) {
            // Check against all active zones
            for (const auto& zone : zones_) {
                if (zone.isActive && isPersonInZone(detection.box, zone.area)) {
                    alerts.emplace_back(zone.name, detection.box, zone.area, detection.confidence);
                }
            }
        }
    }
    
    return alerts;
}

void HazardZoneManager::drawZones(cv::Mat& frame) {
    if (frame.empty() || !showZones_ || !enabled_) return;
    
    std::cout << "Drawing " << zones_.size() << " hazard zones on frame " << frame.cols << "x" << frame.rows << std::endl;
    
    for (const auto& zone : zones_) {
        // Use bright colors like object detection boxes
        cv::Scalar boxColor = zone.isActive ? cv::Scalar(0, 0, 255) : cv::Scalar(128, 128, 128); // Red for active, gray for inactive
        
        std::cout << "Drawing zone: " << zone.name << " at [" << zone.area.x << "," << zone.area.y 
                 << "," << zone.area.width << "," << zone.area.height << "] active: " << zone.isActive << std::endl;
        
        // Draw main rectangle border (like detection box)
        cv::rectangle(frame, zone.area, boxColor, 3);
        
        // Draw label background (like detection labels)
        std::string label = zone.name;
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.8, 2, &baseline);
        
        // Position label at top of box
        cv::Point labelPos(zone.area.x, zone.area.y - 10);
        
        // Ensure label is within frame
        if (labelPos.y < textSize.height + 5) {
            labelPos.y = zone.area.y + textSize.height + 5; // Put inside box if too close to top
        }
        
        // Draw label background (like detection box labels)
        cv::rectangle(frame, 
                     cv::Point(labelPos.x, labelPos.y - textSize.height - 5),
                     cv::Point(labelPos.x + textSize.width + 5, labelPos.y + 5),
                     boxColor, -1);
        
        // Draw label text
        cv::putText(frame, label, cv::Point(labelPos.x + 2, labelPos.y - 3),
                   cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
        
        // Add status indicator in corner
        std::string status = zone.isActive ? "ACTIVE" : "INACTIVE";
        cv::Size statusSize = cv::getTextSize(status, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        cv::Point statusPos(zone.area.x + zone.area.width - statusSize.width - 5, 
                           zone.area.y + statusSize.height + 5);
        
        // Status background
        cv::rectangle(frame,
                     cv::Point(statusPos.x - 2, statusPos.y - statusSize.height - 2),
                     cv::Point(statusPos.x + statusSize.width + 2, statusPos.y + 2),
                     zone.isActive ? cv::Scalar(0, 255, 0) : cv::Scalar(128, 128, 128), -1);
        
        // Status text
        cv::putText(frame, status, statusPos, cv::FONT_HERSHEY_SIMPLEX, 0.5, 
                   cv::Scalar(0, 0, 0), 1);
    }
}

void HazardZoneManager::drawAlerts(cv::Mat& frame, const std::vector<HazardAlert>& alerts) {
    if (frame.empty() || alerts.empty() || !enabled_) return;
    
    for (const auto& alert : alerts) {
        // Draw warning indicators
        cv::rectangle(frame, alert.personBox, cv::Scalar(0, 0, 255), 4); // Thick red border
        
        // Warning text
        std::string warning = "DANGER! Person in " + alert.zoneName;
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(warning, cv::FONT_HERSHEY_SIMPLEX, 0.8, 2, &baseline);
        
        // Background
        cv::rectangle(frame,
                     cv::Point(alert.personBox.x, alert.personBox.y - textSize.height - 15),
                     cv::Point(alert.personBox.x + textSize.width, alert.personBox.y),
                     cv::Scalar(0, 0, 255), -1);
        
        // Warning text
        cv::putText(frame, warning,
                   cv::Point(alert.personBox.x, alert.personBox.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
    }
    
    // Show visual alarm if enabled
    if (visualAlarmEnabled_ && !alerts.empty()) {
        showVisualAlarm(frame);
    }
}

void HazardZoneManager::triggerAlarm(const std::vector<HazardAlert>& alerts) {
    if (!alarmEnabled_ || alerts.empty() || !enabled_) return;
    
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastAlarm = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastAlarmTime_).count();
    
    // Check cooldown
    if (timeSinceLastAlarm < alarmCooldownMs_) return;
    
    lastAlarmTime_ = now;
    currentlyAlarming_ = true;
    
    // Log alarm
    std::cout << "\n🚨 HAZARD ZONE VIOLATION DETECTED! 🚨" << std::endl;
    for (const auto& alert : alerts) {
        std::cout << "⚠️  Person detected in " << alert.zoneName 
                  << " (confidence: " << std::fixed << std::setprecision(1) 
                  << alert.confidence * 100 << "%)" << std::endl;
    }
    std::cout << "==========================================\n" << std::endl;
    
    // Audio alarm
    if (audioAlarmEnabled_) {
        playAlarmSound();
    }
}

bool HazardZoneManager::hasActiveZones() const {
    return std::any_of(zones_.begin(), zones_.end(),
        [](const HazardZone& zone) { return zone.isActive; });
}

bool HazardZoneManager::isPersonInZone(const cv::Rect& personBox, const cv::Rect& zoneArea) {
    // Check if person's center point is in the zone
    cv::Point personCenter(personBox.x + personBox.width / 2, personBox.y + personBox.height / 2);
    bool centerInZone = zoneArea.contains(personCenter);
    
    // Also check for overlap (person partially in zone)
    cv::Rect intersection = personBox & zoneArea;
    bool hasOverlap = intersection.area() > 0;
    
    return centerInZone || hasOverlap;
}

cv::Rect HazardZoneManager::parseRectFromString(const std::string& rectStr) {
    // Expected format: "x,y,width,height" or "x y width height"
    std::stringstream ss(rectStr);
    std::string token;
    std::vector<int> values;
    
    // Try comma separator first
    while (std::getline(ss, token, ',')) {
        values.push_back(std::stoi(token));
    }
    
    // If no commas, try space separator
    if (values.size() != 4) {
        values.clear();
        ss.clear();
        ss.str(rectStr);
        while (ss >> token) {
            values.push_back(std::stoi(token));
        }
    }
    
    if (values.size() != 4) {
        throw std::runtime_error("Invalid rectangle format: " + rectStr + " (expected: x,y,width,height)");
    }
    
    return cv::Rect(values[0], values[1], values[2], values[3]);
}

void HazardZoneManager::playAlarmSound() {
    // Simple beep using system command (Linux)
    system("(speaker-test -t sine -f 1000 -l 1 & sleep 0.2; kill $!) > /dev/null 2>&1");
}

void HazardZoneManager::showVisualAlarm(cv::Mat& frame) {
    // Flash effect - alternate between normal and red overlay
    static int flashCounter = 0;
    flashCounter++;
    
    if (flashCounter % 10 < 5) { // Flash every 5 frames
        cv::Mat overlay;
        frame.copyTo(overlay);
        cv::rectangle(overlay, cv::Rect(0, 0, frame.cols, frame.rows), cv::Scalar(0, 0, 255), -1);
        cv::addWeighted(frame, 0.7, overlay, 0.3, 0, frame);
        
        // Add warning text
        std::string warning = "⚠️ HAZARD ZONE VIOLATION ⚠️";
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(warning, cv::FONT_HERSHEY_SIMPLEX, 1.2, 3, &baseline);
        cv::Point textPos((frame.cols - textSize.width) / 2, 50);
        
        cv::putText(frame, warning, textPos, cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(255, 255, 255), 3);
    }
}

void HazardZoneManager::drawZoneStatus(cv::Mat& frame) {
    if (frame.empty() || !showZoneStatus_ || !enabled_) return;
    
    // Display zone status in top-left corner
    int yPos = 30;
    std::string status = "Hazard Zones: " + std::to_string(zones_.size()) + " configured";
    
    // Background for status
    cv::Size textSize = cv::getTextSize(status, cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, nullptr);
    cv::rectangle(frame, cv::Point(10, 10), cv::Point(textSize.width + 20, yPos + 15), 
                  cv::Scalar(0, 0, 0), -1);
    cv::rectangle(frame, cv::Point(10, 10), cv::Point(textSize.width + 20, yPos + 15), 
                  cv::Scalar(255, 255, 255), 2);
    
    cv::putText(frame, status, cv::Point(15, yPos), cv::FONT_HERSHEY_SIMPLEX, 0.7, 
                cv::Scalar(0, 255, 0), 2);
    
    yPos += 25;
    
    // Show each zone status
    for (size_t i = 0; i < zones_.size(); ++i) {
        const auto& zone = zones_[i];
        std::string zoneInfo = zone.name + ": " + (zone.isActive ? "ACTIVE" : "INACTIVE");
        cv::Scalar color = zone.isActive ? cv::Scalar(0, 255, 0) : cv::Scalar(100, 100, 100);
        
        // Background
        cv::Size zoneTextSize = cv::getTextSize(zoneInfo, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, nullptr);
        cv::rectangle(frame, cv::Point(10, yPos - 15), cv::Point(zoneTextSize.width + 20, yPos + 5), 
                      cv::Scalar(0, 0, 0), -1);
        
        cv::putText(frame, zoneInfo, cv::Point(15, yPos), cv::FONT_HERSHEY_SIMPLEX, 0.5, 
                    color, 1);
        yPos += 20;
    }
} 