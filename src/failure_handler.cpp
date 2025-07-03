#include "include/failure_handler.h"

// --- DetectionFailure Implementation ---

DetectionFailure::DetectionFailure() 
    : selectedStrategyId_(HIGHEST_CONFIDENCE), 
      similarityThreshold_(0.7),
      spatialWeight_(0.4),      // Spatial position is important for tracking
      appearanceWeight_(0.4),   // Appearance similarity is also important
      sizeWeight_(0.2),         // Size should be relatively stable
      maxExpectedMovement_(100.0) {  // Maximum expected movement in pixels
}

void DetectionFailure::setSelectionStrategy(int strategyId) {
    selectedStrategyId_ = strategyId;
    
    if (strategyId == SIMILARITY && (lastFailureFrame_.empty() || lastFailureBox_.area() == 0)) {
        std::cout << "Warning: Similarity strategy selected but no reference available. Will use highest confidence until reference is available." << std::endl;
    }
}

bool DetectionFailure::selectTarget(const std::vector<model::Detection>& detections,
                                    cv::Rect& selectedBox, float& selectedConf, int& selectedClassId) {
    if (detections.empty()) {
        return false;
    }
    
    switch (selectedStrategyId_) {
        case HIGHEST_CONFIDENCE:
            return selectHighestConfidence(detections, selectedBox, selectedConf, selectedClassId);
        case UPPER_BOX:
            return selectUpperBox(detections, selectedBox, selectedConf, selectedClassId);
        case LOWER_BOX:
            return selectLowerBox(detections, selectedBox, selectedConf, selectedClassId);
        case RIGHTMOST_BOX:
            return selectRightmostBox(detections, selectedBox, selectedConf, selectedClassId);
        case LEFTMOST_BOX:
            return selectLeftmostBox(detections, selectedBox, selectedConf, selectedClassId);
        case SIMILARITY:
            return selectSimilarity(detections, selectedBox, selectedConf, selectedClassId);
        default:
            return selectHighestConfidence(detections, selectedBox, selectedConf, selectedClassId);
    }
}

// --- Strategy Implementations ---

bool DetectionFailure::selectHighestConfidence(const std::vector<model::Detection>& detections,
                                               cv::Rect& selectedBox, float& selectedConf, int& selectedClassId) {
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

bool DetectionFailure::selectUpperBox(const std::vector<model::Detection>& detections,
                                      cv::Rect& selectedBox, float& selectedConf, int& selectedClassId) {
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

bool DetectionFailure::selectLowerBox(const std::vector<model::Detection>& detections,
                                      cv::Rect& selectedBox, float& selectedConf, int& selectedClassId) {
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

bool DetectionFailure::selectRightmostBox(const std::vector<model::Detection>& detections,
                                          cv::Rect& selectedBox, float& selectedConf, int& selectedClassId) {
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

bool DetectionFailure::selectLeftmostBox(const std::vector<model::Detection>& detections,
                                         cv::Rect& selectedBox, float& selectedConf, int& selectedClassId) {
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

bool DetectionFailure::selectSimilarity(const std::vector<model::Detection>& detections,
                                        cv::Rect& selectedBox, float& selectedConf, int& selectedClassId) {
    if (lastFailureFrame_.empty() || lastFailureBox_.area() == 0) {
        // Fallback to highest confidence if no reference available
        std::cout << "No reference available for similarity. Falling back to highest confidence." << std::endl;
        return selectHighestConfidence(detections, selectedBox, selectedConf, selectedClassId);
    }
    
    double bestSimilarity = -1.0;
    selectedConf = -1.0f;
    
    for (const auto& det : detections) {
        if (det.confidence > 0.15f) {
            // Use enhanced similarity for better tracking scenarios
            double similarity = calculateEnhancedSimilarity(lastFailureFrame_, det.box);
            if (similarity > bestSimilarity) {
                bestSimilarity = similarity;
                selectedBox = det.box;
                selectedConf = det.confidence;
                selectedClassId = det.classId;
            }
        }
    }
    
    // Only return true if similarity is above threshold
    return bestSimilarity >= similarityThreshold_ && selectedConf > 0.15f;
}

double DetectionFailure::calculateSimilarity(const cv::Mat& frame, const cv::Rect& box) const {
    if (frame.empty() || box.area() == 0) {
        return 0.0;
    }
    
    // Ensure box is within frame bounds
    cv::Rect safeBox = box & cv::Rect(0, 0, frame.cols, frame.rows);
    if (safeBox.area() == 0) {
        return 0.0;
    }
    
    // Crop the region of interest
    cv::Mat roi = frame(safeBox);
    
    // Convert to HSV
    cv::Mat hsv;
    cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
    
    // Calculate histogram
    int h_bins = 50; int s_bins = 60;
    int histSize[] = { h_bins, s_bins };
    float h_ranges[] = { 0, 180 };
    float s_ranges[] = { 0, 256 };
    const float* ranges[] = { h_ranges, s_ranges };
    int channels[] = { 0, 1 };
    
    cv::Mat hist;
    cv::calcHist(&hsv, 1, channels, cv::Mat(), hist, 2, histSize, ranges, true, false);
    cv::normalize(hist, hist, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());
    
    // Calculate similarity with reference
    cv::Mat refRoi = lastFailureFrame_(lastFailureBox_);
    cv::Mat refHsv;
    cv::cvtColor(refRoi, refHsv, cv::COLOR_BGR2HSV);
    
    cv::Mat refHist;
    cv::calcHist(&refHsv, 1, channels, cv::Mat(), refHist, 2, histSize, ranges, true, false);
    cv::normalize(refHist, refHist, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());
    
    // Compare histograms using correlation
    return cv::compareHist(hist, refHist, cv::HISTCMP_CORREL);
}

double DetectionFailure::calculateEnhancedSimilarity(const cv::Mat& frame, const cv::Rect& box) const {
    if (frame.empty() || box.area() == 0 || lastFailureFrame_.empty() || lastFailureBox_.area() == 0) {
        return 0.0;
    }
    
    // Calculate individual similarity components
    double spatialSim = calculateSpatialSimilarity(box);
    double appearanceSim = calculateAppearanceSimilarity(frame, box);
    double sizeSim = calculateSizeSimilarity(box);
    
    // Combine similarities with weights
    double combinedSimilarity = spatialWeight_ * spatialSim + 
                               appearanceWeight_ * appearanceSim + 
                               sizeWeight_ * sizeSim;
    
    // Debug logging for the best match
    static int debugCounter = 0;
    if (debugCounter++ % 10 == 0) { // Log every 10th calculation to avoid spam
        std::cout << "Enhanced Similarity - Spatial: " << spatialSim 
                  << ", Appearance: " << appearanceSim 
                  << ", Size: " << sizeSim 
                  << ", Combined: " << combinedSimilarity << std::endl;
    }
    
    return combinedSimilarity;
}

double DetectionFailure::calculateSpatialSimilarity(const cv::Rect& box) const {
    // Calculate center points
    cv::Point2f refCenter(lastFailureBox_.x + lastFailureBox_.width / 2.0f, 
                         lastFailureBox_.y + lastFailureBox_.height / 2.0f);
    cv::Point2f newCenter(box.x + box.width / 2.0f, 
                         box.y + box.height / 2.0f);
    
    // Calculate distance between centers
    double distance = cv::norm(refCenter - newCenter);
    
    // Convert distance to similarity (closer = higher similarity)
    // Use exponential decay based on maxExpectedMovement
    double spatialSimilarity = std::exp(-distance / maxExpectedMovement_);
    
    return spatialSimilarity;
}

double DetectionFailure::calculateAppearanceSimilarity(const cv::Mat& frame, const cv::Rect& box) const {
    // Ensure boxes are within frame bounds
    cv::Rect safeBox = box & cv::Rect(0, 0, frame.cols, frame.rows);
    cv::Rect safeRefBox = lastFailureBox_ & cv::Rect(0, 0, lastFailureFrame_.cols, lastFailureFrame_.rows);
    
    if (safeBox.area() == 0 || safeRefBox.area() == 0) {
        return 0.0;
    }
    
    // Crop regions of interest
    cv::Mat roi = frame(safeBox);
    cv::Mat refRoi = lastFailureFrame_(safeRefBox);
    
    // Resize to same size for comparison (use smaller size for efficiency)
    cv::Size targetSize(64, 64);
    cv::Mat resizedRoi, resizedRefRoi;
    cv::resize(roi, resizedRoi, targetSize);
    cv::resize(refRoi, resizedRefRoi, targetSize);
    
    // Convert to grayscale for structural similarity
    cv::Mat grayRoi, grayRefRoi;
    cv::cvtColor(resizedRoi, grayRoi, cv::COLOR_BGR2GRAY);
    cv::cvtColor(resizedRefRoi, grayRefRoi, cv::COLOR_BGR2GRAY);
    
    // Calculate structural similarity (SSIM-like measure)
    cv::Mat diff;
    cv::absdiff(grayRoi, grayRefRoi, diff);
    
    // Convert to similarity measure (lower diff = higher similarity)
    double meanDiff = cv::mean(diff)[0];
    double maxPossibleDiff = 255.0;
    double structuralSimilarity = 1.0 - (meanDiff / maxPossibleDiff);
    
    // Also calculate histogram similarity as backup
    cv::Mat histRoi, histRefRoi;
    int histSize[] = { 50, 60 };
    float h_ranges[] = { 0, 180 };
    float s_ranges[] = { 0, 256 };
    const float* ranges[] = { h_ranges, s_ranges };
    int channels[] = { 0, 1 };
    
    cv::Mat hsvRoi, hsvRefRoi;
    cv::cvtColor(resizedRoi, hsvRoi, cv::COLOR_BGR2HSV);
    cv::cvtColor(resizedRefRoi, hsvRefRoi, cv::COLOR_BGR2HSV);
    
    cv::calcHist(&hsvRoi, 1, channels, cv::Mat(), histRoi, 2, histSize, ranges, true, false);
    cv::calcHist(&hsvRefRoi, 1, channels, cv::Mat(), histRefRoi, 2, histSize, ranges, true, false);
    
    cv::normalize(histRoi, histRoi, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());
    cv::normalize(histRefRoi, histRefRoi, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());
    
    double histogramSimilarity = cv::compareHist(histRoi, histRefRoi, cv::HISTCMP_CORREL);
    
    // Combine structural and histogram similarity
    double appearanceSimilarity = 0.6 * structuralSimilarity + 0.4 * histogramSimilarity;
    
    return std::max(0.0, appearanceSimilarity); // Ensure non-negative
}

double DetectionFailure::calculateSizeSimilarity(const cv::Rect& box) const {
    // Calculate areas
    double refArea = lastFailureBox_.width * lastFailureBox_.height;
    double newArea = box.width * box.height;
    
    if (refArea <= 0 || newArea <= 0) {
        return 0.0;
    }
    
    // Calculate size ratio
    double sizeRatio = std::min(refArea, newArea) / std::max(refArea, newArea);
    
    // Convert to similarity (closer ratio to 1.0 = higher similarity)
    // Use a tolerance for size changes (objects can grow/shrink slightly)
    double sizeSimilarity = sizeRatio;
    
    return sizeSimilarity;
}

bool DetectionFailure::areObjectsSimilar(const cv::Mat& lastFrame, const cv::Rect& lastBox,
                                         const cv::Mat& newFrame, const cv::Rect& newBox,
                                         double similarityThreshold) const {
    if (lastFrame.empty() || newFrame.empty() || lastBox.area() == 0 || newBox.area() == 0) {
        return false;
    }

    // Crop the regions of interest
    cv::Rect safeLastBox = lastBox & cv::Rect(0, 0, lastFrame.cols, lastFrame.rows);
    cv::Rect safeNewBox = newBox & cv::Rect(0, 0, newFrame.cols, newFrame.rows);
    
    if (safeLastBox.area() == 0 || safeNewBox.area() == 0) {
        return false;
    }
    
    cv::Mat lastRoi = lastFrame(safeLastBox);
    cv::Mat newRoi = newFrame(safeNewBox);

    // Convert to HSV
    cv::Mat hsvLast, hsvNew;
    cv::cvtColor(lastRoi, hsvLast, cv::COLOR_BGR2HSV);
    cv::cvtColor(newRoi, hsvNew, cv::COLOR_BGR2HSV);

    // Calculate histograms
    int h_bins = 50; int s_bins = 60;
    int histSize[] = { h_bins, s_bins };
    float h_ranges[] = { 0, 180 };
    float s_ranges[] = { 0, 256 };
    const float* ranges[] = { h_ranges, s_ranges };
    int channels[] = { 0, 1 };

    cv::Mat histLast, histNew;
    cv::calcHist(&hsvLast, 1, channels, cv::Mat(), histLast, 2, histSize, ranges, true, false);
    cv::normalize(histLast, histLast, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());
    
    cv::calcHist(&hsvNew, 1, channels, cv::Mat(), histNew, 2, histSize, ranges, true, false);
    cv::normalize(histNew, histNew, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());

    // Compare histograms
    double similarity = cv::compareHist(histLast, histNew, cv::HISTCMP_CORREL);

    return similarity >= similarityThreshold;
}

void DetectionFailure::updateSimilarityReference(const cv::Mat& frame, const cv::Rect& box) {
    lastFailureFrame_ = frame.clone();
    lastFailureBox_ = box;
    
    if (selectedStrategyId_ == SIMILARITY) {
        std::cout << "Similarity strategy reference updated." << std::endl;
    }
}

void DetectionFailure::configureSimilarityWeights(double spatialWeight, double appearanceWeight, double sizeWeight, double maxMovement) {
    spatialWeight_ = spatialWeight;
    appearanceWeight_ = appearanceWeight;
    sizeWeight_ = sizeWeight;
    maxExpectedMovement_ = maxMovement;
    
    // Normalize weights to sum to 1.0
    double totalWeight = spatialWeight_ + appearanceWeight_ + sizeWeight_;
    if (totalWeight > 0) {
        spatialWeight_ /= totalWeight;
        appearanceWeight_ /= totalWeight;
        sizeWeight_ /= totalWeight;
    }
    
    std::cout << "Similarity weights configured - Spatial: " << spatialWeight_ 
              << ", Appearance: " << appearanceWeight_ 
              << ", Size: " << sizeWeight_ 
              << ", Max Movement: " << maxExpectedMovement_ << std::endl;
}

void DetectionFailure::setSimilarityThreshold(double threshold) {
    similarityThreshold_ = threshold;
    std::cout << "Similarity threshold set to: " << similarityThreshold_ << std::endl;
}

std::string DetectionFailure::getStrategyName(int strategyId) {
    switch (strategyId) {
        case HIGHEST_CONFIDENCE: return "Highest Confidence";
        case UPPER_BOX: return "Upper Box";
        case LOWER_BOX: return "Lower Box";
        case RIGHTMOST_BOX: return "Rightmost Box";
        case LEFTMOST_BOX: return "Leftmost Box";
        case SIMILARITY: return "Similarity";
        default: return "Unknown Strategy";
    }
}

int DetectionFailure::getCurrentStrategyId() const {
    return selectedStrategyId_;
}

std::string DetectionFailure::getCurrentStrategyName() const {
    return getStrategyName(selectedStrategyId_);
} 