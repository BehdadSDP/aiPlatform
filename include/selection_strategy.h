#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include "model.h"

// Abstract base class for selection strategies
class SelectionStrategy {
public:
    virtual ~SelectionStrategy() = default;
    virtual bool selectDetection(const std::vector<model::Detection>& detections,
                               cv::Rect& selectedBox,
                               float& selectedConf,
                               int& selectedClassId) = 0;
    virtual std::string getName() const = 0;
};

// Highest confidence strategy
class HighestConfidenceStrategy : public SelectionStrategy {
public:
    bool selectDetection(const std::vector<model::Detection>& detections,
                        cv::Rect& selectedBox,
                        float& selectedConf,
                        int& selectedClassId) override {
        selectedConf = -1.0f;
        for (const auto& det : detections) {
            if (det.confidence > selectedConf) {
                selectedConf = det.confidence;
                selectedBox = det.box;
                selectedClassId = det.classId;
            }
        }
        return selectedConf > 0.15f;
    }

    std::string getName() const override { return "Highest Confidence"; }
};

// Upper bounding box strategy
class UpperBoundingBoxStrategy : public SelectionStrategy {
public:
    bool selectDetection(const std::vector<model::Detection>& detections,
                        cv::Rect& selectedBox,
                        float& selectedConf,
                        int& selectedClassId) override {
        int minY = INT_MAX;
        selectedConf = -1.0f;
        
        for (const auto& det : detections) {
            if (det.confidence > 0.15f && det.box.y < minY) {
                minY = det.box.y;
                selectedBox = det.box;
                selectedConf = det.confidence;
                selectedClassId = det.classId;
            }
        }
        return selectedConf > 0.15f;
    }

    std::string getName() const override { return "Upper Bounding Box"; }
};

// Lower bounding box strategy
class LowerBoundingBoxStrategy : public SelectionStrategy {
public:
    bool selectDetection(const std::vector<model::Detection>& detections,
                        cv::Rect& selectedBox,
                        float& selectedConf,
                        int& selectedClassId) override {
        int maxY = -1;
        selectedConf = -1.0f;
        
        for (const auto& det : detections) {
            if (det.confidence > 0.15f && det.box.y > maxY) {
                maxY = det.box.y;
                selectedBox = det.box;
                selectedConf = det.confidence;
                selectedClassId = det.classId;
            }
        }
        return selectedConf > 0.15f;
    }

    std::string getName() const override { return "Lower Bounding Box"; }
};

// Rightmost bounding box strategy
class RightmostBoundingBoxStrategy : public SelectionStrategy {
public:
    bool selectDetection(const std::vector<model::Detection>& detections,
                        cv::Rect& selectedBox,
                        float& selectedConf,
                        int& selectedClassId) override {
        int maxX = -1;
        selectedConf = -1.0f;
        
        for (const auto& det : detections) {
            if (det.confidence > 0.15f && det.box.x + det.box.width > maxX) {
                maxX = det.box.x + det.box.width;
                selectedBox = det.box;
                selectedConf = det.confidence;
                selectedClassId = det.classId;
            }
        }
        return selectedConf > 0.15f;
    }

    std::string getName() const override { return "Rightmost Bounding Box"; }
};

// Leftmost bounding box strategy
class LeftmostBoundingBoxStrategy : public SelectionStrategy {
public:
    bool selectDetection(const std::vector<model::Detection>& detections,
                        cv::Rect& selectedBox,
                        float& selectedConf,
                        int& selectedClassId) override {
        int minX = INT_MAX;
        selectedConf = -1.0f;
        
        for (const auto& det : detections) {
            if (det.confidence > 0.15f && det.box.x < minX) {
                minX = det.box.x;
                selectedBox = det.box;
                selectedConf = det.confidence;
                selectedClassId = det.classId;
            }
        }
        return selectedConf > 0.15f;
    }

    std::string getName() const override { return "Leftmost Bounding Box"; }
};

// Factory class to create selection strategies
class SelectionStrategyFactory {
public:
    static std::unique_ptr<SelectionStrategy> createStrategy(int strategyId) {
        switch (strategyId) {
            case 0:
                return std::make_unique<HighestConfidenceStrategy>();
            case 1:
                return std::make_unique<UpperBoundingBoxStrategy>();
            case 2:
                return std::make_unique<LowerBoundingBoxStrategy>();
            case 3:
                return std::make_unique<RightmostBoundingBoxStrategy>();
            case 4:
                return std::make_unique<LeftmostBoundingBoxStrategy>();
            default:
                return std::make_unique<HighestConfidenceStrategy>();
        }
    }
}; 