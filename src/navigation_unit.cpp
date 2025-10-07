#include "include/navigation_unit.h"
#include "include/mavlink.h" // Include the full header for implementation
#include <iostream>
#include <algorithm> // For std::clamp

NavigationUnit::NavigationUnit() {
    m_lastUpdateTime = std::chrono::steady_clock::now();
}

void NavigationUnit::setMavlink(Mavlink* mavlink) {
    m_mavlink = mavlink;
}

void NavigationUnit::setRollPIDGains(float kp, float ki, float kd) {
    m_rollKp = kp;
    m_rollKi = ki;
    m_rollKd = kd;
}

void NavigationUnit::setPitchPIDGains(float kp, float ki, float kd) {
    m_pitchKp = kp;
    m_pitchKi = ki;
    m_pitchKd = kd;
}

void NavigationUnit::resetPIDStates() {
    m_rollIntegral = 0.0f;
    m_rollPreviousError = 0.0f;
    m_pitchIntegral = 0.0f;
    m_pitchPreviousError = 0.0f;
    m_firstUpdate = true;
    
    // Reset filter state as well
    m_filteredError = cv::Point2f(0.0f, 0.0f);
    m_firstFilterUpdate = true;
}

void NavigationUnit::setFilterAlpha(float alpha) {
    // Clamp alpha to valid range [0.0, 1.0]
    m_filterAlpha = std::clamp(alpha, 0.0f, 1.0f);
}

cv::Point2f NavigationUnit::applyLowPassFilter(const cv::Point2f& rawError) {
    if (m_firstFilterUpdate) {
        // For the first update, initialize the filter with the raw error
        m_filteredError = rawError;
        m_firstFilterUpdate = false;
        return m_filteredError;
    }
    
    // Apply Exponential Moving Average formula:
    // new_filtered_error = (α * current_raw_error) + ((1 - α) * previous_filtered_error)
    m_filteredError.x = (m_filterAlpha * rawError.x) + ((1.0f - m_filterAlpha) * m_filteredError.x);
    m_filteredError.y = (m_filterAlpha * rawError.y) + ((1.0f - m_filterAlpha) * m_filteredError.y);
    
    return m_filteredError;
}

float NavigationUnit::computePID(float error, float kp, float ki, float kd, 
                                 float& integral, float& previousError, float deltaTime) {
    // Proportional term
    float proportional = kp * error;
    
    // Integral term with windup protection
    integral += error * deltaTime;
    integral = std::clamp(integral, -m_integralMax, m_integralMax);
    float integralTerm = ki * integral;
    
    // Derivative term
    float derivative = 0.0f;
    if (!m_firstUpdate && deltaTime > 0.0f) {
        derivative = (error - previousError) / deltaTime;
    }
    float derivativeTerm = kd * derivative;
    
    // Store current error for next iteration
    previousError = error;
    
    return proportional + integralTerm + derivativeTerm;
}

cv::Point2f NavigationUnit::calculateError(const cv::Rect& objectBox, int frameWidth, int frameHeight) {
// ...existing code...
    cv::Point2f frameCenter(frameWidth / 2.0f, frameHeight / 2.0f);

    // Calculate the center of the tracked object's bounding box
    cv::Point2f objectCenter(objectBox.x + objectBox.width / 2.0f, objectBox.y + objectBox.height / 2.0f);

    // Calculate the error (distance) between the two centers
    cv::Point2f error = objectCenter - frameCenter;

    return error;
}

ControlOutputs NavigationUnit::generateControlCommands(const cv::Point2f& error) {
    ControlOutputs outputs;
    
    // Apply low-pass filter to smooth the error signal
    cv::Point2f filteredError = applyLowPassFilter(error);
    
    // Store both raw and filtered error for debugging
    outputs.error = filteredError;  // Use filtered error for control
    
    // Calculate delta time for PID controller
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = 0.0f;
    
    if (!m_firstUpdate) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_lastUpdateTime);
        deltaTime = duration.count() / 1000000.0f; // Convert to seconds
    }
    
    m_lastUpdateTime = currentTime;
    
    // Compute PID outputs using filtered error for roll and pitch
    float rollPIDOutput = computePID(filteredError.x, m_rollKp, m_rollKi, m_rollKd, 
                                     m_rollIntegral, m_rollPreviousError, deltaTime);
    float pitchPIDOutput = computePID(-filteredError.y, m_pitchKp, m_pitchKi, m_pitchKd, 
                                      m_pitchIntegral, m_pitchPreviousError, deltaTime);
    
    m_firstUpdate = false;
    
    // Convert PID outputs to RC commands (1000-2000)
    const int neutral = 1500;
    const int max_deviation = 200;
    
    // Apply PID outputs to neutral position
    int roll_output = static_cast<int>(neutral + rollPIDOutput);
    int pitch_output = static_cast<int>(neutral + pitchPIDOutput);
    
    // Clamp the values to a safe range
    roll_output = std::clamp(roll_output, neutral - max_deviation, neutral + max_deviation);
    pitch_output = std::clamp(pitch_output, neutral - max_deviation, neutral + max_deviation);

    // Store outputs for debugging
    outputs.roll_output = roll_output;
    outputs.pitch_output = pitch_output;
    outputs.rc_commands_sent = true;  // Default behavior - always send commands
    m_lastOutputs = outputs;

    // Send RC override command only if MAVLink is available
    if (m_mavlink) {
        // Create the RC override command
        uint16_t channels[18] = {0};
        channels[0] = roll_output;    // Roll
        channels[1] = pitch_output;   // Pitch

        // Set remaining channels to "ignore"
        for (int i = 2; i < 18; ++i) {
            channels[i] = UINT16_MAX;
        }

        // Send the command
        bool success = m_mavlink->sendRCOverride(channels);
    } else {
        std::cerr << "ERROR: NavigationUnit: MAVLink system not set, cannot send RC override" << std::endl;
    }
    
    return outputs;
}

ControlOutputs NavigationUnit::generateControlCommands(const cv::Point2f& error, const cv::Rect& objectBox, int frameWidth, int frameHeight) {
    ControlOutputs outputs;
    
    // Apply low-pass filter to smooth the error signal
    cv::Point2f filteredError = applyLowPassFilter(error);
    
    // Store both raw and filtered error for debugging
    outputs.error = filteredError;  // Use filtered error for control
    
    // Calculate delta time for PID controller
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = 0.0f;
    
    if (!m_firstUpdate) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_lastUpdateTime);
        deltaTime = duration.count() / 1000000.0f; // Convert to seconds
    }
    
    m_lastUpdateTime = currentTime;
    
    // Compute PID outputs using filtered error for roll and pitch
    float rollPIDOutput = computePID(filteredError.x, m_rollKp, m_rollKi, m_rollKd, 
                                     m_rollIntegral, m_rollPreviousError, deltaTime);
    float pitchPIDOutput = computePID(-filteredError.y, m_pitchKp, m_pitchKi, m_pitchKd, 
                                      m_pitchIntegral, m_pitchPreviousError, deltaTime);
    
    m_firstUpdate = false;
    
    // Convert PID outputs to RC commands (1000-2000)
    const int neutral = 1500;
    const int max_deviation = 200;
    
    // Apply PID outputs to neutral position
    int roll_output = static_cast<int>(neutral + rollPIDOutput);
    int pitch_output = static_cast<int>(neutral + pitchPIDOutput);
    
    // Clamp the values to a safe range
    roll_output = std::clamp(roll_output, neutral - max_deviation, neutral + max_deviation);
    pitch_output = std::clamp(pitch_output, neutral - max_deviation, neutral + max_deviation);

    // Store outputs for debugging
    outputs.roll_output = roll_output;
    outputs.pitch_output = pitch_output;
    
    // Check if frame center is inside the bounding box
    bool centerInBox = isFrameCenterInBoundingBox(objectBox, frameWidth, frameHeight);
    outputs.rc_commands_sent = !centerInBox;  // Only send commands if center is outside box
    
    m_lastOutputs = outputs;

    // Send RC override command only if center is outside bounding box AND MAVLink is available
    if (!centerInBox && m_mavlink) {
        // Create the RC override command
        uint16_t channels[18] = {0};
        channels[0] = roll_output;    // Roll
        channels[1] = pitch_output;   // Pitch

        // Set remaining channels to "ignore"
        for (int i = 2; i < 18; ++i) {
            channels[i] = UINT16_MAX;
        }

        // Send the command
        bool success = m_mavlink->sendRCOverride(channels);
    } else if (centerInBox) {
        // Frame center is inside bounding box - object is centered, no RC commands needed
        // PID calculations continue for smooth transitions when object moves away from center
    } else {
        std::cerr << "ERROR: NavigationUnit: MAVLink system not set, cannot send RC override" << std::endl;
    }
    
    return outputs;
}

bool NavigationUnit::isFrameCenterInBoundingBox(const cv::Rect& objectBox, int frameWidth, int frameHeight) {
    // Calculate the center point of the frame
    cv::Point2f frameCenter(frameWidth / 2.0f, frameHeight / 2.0f);
    
    // Check if the frame center point is inside the bounding box
    bool insideX = (frameCenter.x >= objectBox.x) && (frameCenter.x <= (objectBox.x + objectBox.width));
    bool insideY = (frameCenter.y >= objectBox.y) && (frameCenter.y <= (objectBox.y + objectBox.height));
    
    return insideX && insideY;
}

bool NavigationUnit::armVehicle(bool arm, bool force) {
    if (!m_mavlink) {
        std::cerr << "NavigationUnit: MAVLink system not set, cannot send arm command" << std::endl;
        return false;
    }

    // Send the arm/disarm command through MAVLink
    bool success = m_mavlink->armDisarm(arm, force);
    
    return success;
}

const ControlOutputs& NavigationUnit::getLastControlOutputs() const {
    return m_lastOutputs;
}
