#include "include/navigation_unit.h"
#include "include/mavlink.h" // Include the full header for implementation
#include "include/logger.h"
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

void NavigationUnit::setYawPIDGains(float kp, float ki, float kd) {
    m_yawKp = kp;
    m_yawKi = ki;
    m_yawKd = kd;
}

void NavigationUnit::resetPIDStates() {
    m_rollIntegral = 0.0f;
    m_rollPreviousError = 0.0f;
    m_pitchIntegral = 0.0f;
    m_pitchPreviousError = 0.0f;
    m_yawIntegral = 0.0f;
    m_yawPreviousError = 0.0f;
    m_firstUpdate = true;
    
    // Reset filter state as well
    m_filteredError = cv::Point2f(0.0f, 0.0f);
    m_firstFilterUpdate = true;
}

void NavigationUnit::setFilterAlpha(float alpha) {
    // Clamp alpha to valid range [0.0, 1.0]
    m_filterAlpha = std::clamp(alpha, 0.0f, 1.0f);
}

void NavigationUnit::setMaxRCChangeRate(float maxChangeRate) {
    // Clamp to reasonable range (5-500 PWM units per second)
    m_maxRCChangeRate = std::clamp(maxChangeRate, 5.0f, 500.0f);
}

void NavigationUnit::setCenteringRadius(float radius) {
    // Clamp to reasonable range (10-200 pixels)
    m_centeringRadius = std::clamp(radius, 10.0f, 200.0f);
}

float NavigationUnit::getCenteringRadius() const {
    return m_centeringRadius;
}

void NavigationUnit::setYawDeadZoneWidth(float width) {
    // Clamp to reasonable range (20-400 pixels)
    m_yawDeadZoneWidth = std::clamp(width, 20.0f, 400.0f);
}

float NavigationUnit::getYawDeadZoneWidth() const {
    return m_yawDeadZoneWidth;
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

int NavigationUnit::applySlewRateLimit(int desiredOutput, int previousOutput, float deltaTime) {
    if (m_firstOutputUpdate || deltaTime <= 0.0f) {
        // For first update or invalid time, return desired output directly
        return desiredOutput;
    }
    
    // Calculate maximum allowed change based on time elapsed
    float maxChange = m_maxRCChangeRate * deltaTime;
    
    // Calculate the actual change requested
    int outputDifference = desiredOutput - previousOutput;
    
    // Limit the change to the maximum allowed
    if (std::abs(outputDifference) > maxChange) {
        // Apply rate limiting
        if (outputDifference > 0) {
            return previousOutput + static_cast<int>(maxChange);
        } else {
            return previousOutput - static_cast<int>(maxChange);
        }
    }
    
    // If change is within limit, return desired output
    return desiredOutput;
}

float NavigationUnit::computePID(float error, float kp, float ki, float kd, 
                                 float& integral, float& previousError, float deltaTime, float deadZone) {
    // If in dead zone, reset integral and return zero
    // This ensures the drone stops moving when object is centered
    if (std::abs(error) < deadZone) {
        // integral = 0.0f;  // Reset integral to prevent drift
        integral += error * deltaTime;  // Accumulate integral even in dead zone (for testing)
        integral = std::clamp(integral, -m_integralMax, m_integralMax);
        previousError = error;
        return ki * integral;  // Return only integral term in dead zone
    }
    
    // Only accumulate integral when OUTSIDE dead zone
    integral += error * deltaTime;
    integral = std::clamp(integral, -m_integralMax, m_integralMax);

    // Proportional term
    float proportional = kp * error;

    // Derivative term
    float derivative = 0.0f;
    if (!m_firstUpdate && deltaTime > 0.0f) {
        derivative = (error - previousError) / deltaTime;
    }
    float derivativeTerm = kd * derivative;
    
    // Integral term
    float integralTerm = ki * integral;
    
    // Store current error for next iteration
    previousError = error;
    
    // Return full PID output
    return proportional + integralTerm + derivativeTerm;
}

cv::Point2f NavigationUnit::calculateError(const cv::Rect& objectBox, int frameWidth, int frameHeight) {

    cv::Point2f frameCenter(frameWidth / 2.0f, frameHeight / 2.0f);

    // Calculate the center of the tracked object's bounding box
    cv::Point2f objectCenter(objectBox.x + objectBox.width / 2.0f, objectBox.y + objectBox.height / 2.0f);

    // Calculate the error (distance) between the two centers
    cv::Point2f error = objectCenter - frameCenter;

    return error;
}

ControlOutputs NavigationUnit::generateControlCommands(const cv::Point2f& error, const cv::Rect& objectBox, int frameWidth, int frameHeight) {
    
    ControlOutputs outputs;
    cv::Point2f filteredError = applyLowPassFilter(error);
    outputs.error = filteredError;

    // Calculate delta time for PID controller
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = 0.0f;
    
    if (!m_firstUpdate) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_lastUpdateTime);
        deltaTime = duration.count() / 1000000.0f; // Convert to seconds
    }
    
    m_lastUpdateTime = currentTime;
    
    // Check if we're in the dead zone (both pitch and yaw within tolerance)
    bool pitchInDeadZone = std::abs(filteredError.y) < (m_centeringRadius / 2.0f);
    bool yawInDeadZone = std::abs(filteredError.x) < (m_yawDeadZoneWidth / 2.0f);
    bool inDeadZone = pitchInDeadZone && yawInDeadZone;
    
    // Debug logging for dead zone detection
    if (inDeadZone) {
        LOG_DEBUG("IN DEADZONE - Pitch error: {:.1f} (limit: {:.1f}), Yaw error: {:.1f} (limit: {:.1f})", 
                  filteredError.y, m_centeringRadius / 2.0f, 
                  filteredError.x, m_yawDeadZoneWidth / 2.0f);
    }
    
    // Compute PID outputs using filtered error for roll and pitch
    float pitchPIDOutput = computePID(filteredError.y, m_pitchKp, m_pitchKi,m_pitchKd,
                                      m_pitchIntegral, m_pitchPreviousError, deltaTime, m_centeringRadius / 2.0f);
    
    // Compute YAW PID output (using horizontal error for yaw control)
    float yawPIDOutput = computePID(filteredError.x, m_yawKp, m_yawKi, m_yawKd,
                                    m_yawIntegral, m_yawPreviousError, deltaTime, m_yawDeadZoneWidth / 2.0f);

    m_firstUpdate = false;
    
    // Convert PID outputs to attitude angles (radians)
    // Scale PID output (in pixels) to angle (in radians)
    // Formula: angle = (error_pixels / frame_height) * max_angle
    // This gives proportional response: larger error = larger angle, up to max
    const float max_angle = 0.087f;  // radians (~5 degrees) - max safe tilt
    
    // Normalize PID output to angle
    // PID output is roughly proportional to error in pixels
    // Assume frame height ~720, max error ~360 pixels
    // Scale so that max error produces max angle
    const float error_to_angle_scale = max_angle / 360.0f;  // ~0.00024 rad/pixel
    
    float pitch_angle = pitchPIDOutput * error_to_angle_scale;
    float yaw_angle = yawPIDOutput * error_to_angle_scale;
    
    // Clamp angles to safe limits
    pitch_angle = std::clamp(pitch_angle, -max_angle, max_angle);
    yaw_angle = std::clamp(yaw_angle, -max_angle, max_angle) + 3.14/2;
    
    // Store outputs as degrees (for easier display)
    // Convert radians to degrees: rad * (180/π) ≈ rad * 57.3
    outputs.pitch_output = static_cast<int>(pitch_angle * 57.3f);  // degrees
    outputs.yaw_output = static_cast<int>(yaw_angle * 57.3f);      // degrees
    outputs.rc_commands_sent = true;
    outputs.in_dead_zone = inDeadZone;
    
    // Send attitude target via MAVLink
    if (m_mavlink) {
        // Use attitude control mode (use_rates = false)
        // ArduPilot does NOT support body rates in SET_ATTITUDE_TARGET
        // Must use quaternion attitude control instead
        float roll_angle = 0.0f;  // No roll
        float thrust = 0.59;      // 0.5 = maintain altitude (if GUID_OPTIONS=0)
        
        // Testing: Only pitch control, yaw disabled
        float test_yaw = 0.0f;
     yaw_angle = 90.0f;
        LOG_DEBUG("Sending attitude target - Pitch: {:.3f} rad, Yaw: {:.3f} rad", 
                  pitch_angle, test_yaw);
        
        (void)m_mavlink->sendAttitudeTarget(0.0f, 0.0f, 0.0f, thrust, false, roll_angle, pitch_angle, yaw_angle);
    } else {
        LOG_ERROR("NavigationUnit: MAVLink system not set, cannot send attitude target");
    }

    // Store last outputs for retrieval
    m_lastOutputs = outputs;
    return outputs;
}

bool NavigationUnit::isFrameCenterInBoundingBox(const cv::Rect& objectBox, int frameWidth, int frameHeight) {
    // Calculate the center point of the frame
    cv::Point2f frameCenter(frameWidth / 2.0f, frameHeight / 2.0f);
    
    // Calculate the center of the tracked object's bounding box
    cv::Point2f objectCenter(objectBox.x + objectBox.width / 2.0f, 
                             objectBox.y + objectBox.height / 2.0f);
    
    // Calculate distance between frame center and object center
    // float dx = frameCenter.x - objectCenter.x;
    float dy = frameCenter.y - objectCenter.y;
    // float distance = std::sqrt(dx * dx + dy * dy);
    float distance = std::abs(dy);
    // Check if frame center is within the circular tolerance zone
    return distance <= m_centeringRadius;
}

bool NavigationUnit::isObjectInYawDeadZone(const cv::Rect& objectBox, int frameWidth, int frameHeight) {
    // Calculate the center point of the frame
    cv::Point2f frameCenter(frameWidth / 2.0f, 2.0f * (frameHeight / 3.0f));
    
    // Calculate the center of the tracked object's bounding box
    cv::Point2f objectCenter(objectBox.x + objectBox.width / 2.0f, 
                             objectBox.y + objectBox.height / 2.0f);
    
    // Calculate horizontal distance between frame center and object center
    float dx = std::abs(frameCenter.x - objectCenter.x);
    
    // Check if object center is within the vertical dead zone around frame center
    // The dead zone extends +/- (m_yawDeadZoneWidth / 2) horizontally from frame center
    return dx <= (m_yawDeadZoneWidth / 2.0f);
}

bool NavigationUnit::armVehicle(bool arm, bool force) {
    if (!m_mavlink) {
        LOG_ERROR("NavigationUnit: MAVLink system not set, cannot send arm command");
        return false;
    }

    // Send the arm/disarm command through MAVLink
    bool success = m_mavlink->armDisarm(arm, force);
    
    return success;
}

const ControlOutputs& NavigationUnit::getLastControlOutputs() const {
    return m_lastOutputs;
}
