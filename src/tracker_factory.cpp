#include "include/tracker_factory.h"
#include "include/vittracker.h"
#include "include/siamfc_pp_tracker.h"
#include <iostream>
#include <stdexcept>

std::unique_ptr<TrackerInterface> TrackerFactory::createTracker(const TrackerConfig& config) {
    switch (config.type) {
        case TrackerType::VIT_TRACKER:
            return std::make_unique<VitTracker>(config.vitModelPath);
            
        case TrackerType::SIAMFC_TRACKER: {
            auto tracker = std::make_unique<SiamFCPPTracker2>();
            if (!tracker->loadModel(config.siamfcFeatureModelPath, config.siamfcTrackingModelPath)) {
                throw std::runtime_error("Failed to load SiamFCPP tracker models");
            }
            return tracker;
        }
            
        default:
            throw std::runtime_error("Unknown tracker type");
    }
} 