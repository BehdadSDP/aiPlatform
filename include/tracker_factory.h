#pragma once

#include "tracker_interface.h"
#include <memory>
#include <string>

enum class TrackerType {
    VIT_TRACKER = 0,
    SIAMFC_TRACKER = 1
};

struct TrackerConfig {
    TrackerType type;
    std::string vitModelPath;
    std::string siamfcFeatureModelPath;
    std::string siamfcTrackingModelPath;
};

class TrackerFactory {
public:
    static std::unique_ptr<TrackerInterface> createTracker(const TrackerConfig& config);
}; 