#include "include/tracker_factory.h"
#include "include/vit_tracker_adapter.h"
#include "include/siamfc_tracker_adapter.h"
#include <iostream>
#include <stdexcept>

std::unique_ptr<TrackerInterface> TrackerFactory::createTracker(const TrackerConfig& config) {
    switch (config.type) {
        case TrackerType::VIT_TRACKER:
            std::cout << "Creating VitTracker" << std::endl;
            return std::make_unique<VitTrackerAdapter>(config.vitModelPath);
            
        case TrackerType::SIAMFC_TRACKER:
            std::cout << "Creating SiamFCPP tracker" << std::endl;
            return std::make_unique<SiamFCPPAdapter>(config.siamfcFeatureModelPath,
                                                    config.siamfcTrackingModelPath);
            
        default:
            throw std::runtime_error("Unknown tracker type");
    }
} 