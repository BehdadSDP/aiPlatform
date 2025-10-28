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
    cv::Point2f frameCenter(frameWidth / 2.0f, 2.0f* (frameHeight / 3.0f));

    // Calculate the center of the tracked object's bounding box
    cv::Point2f objectCenter(objectBox.x + objectBox.width / 2.0f, objectBox.y + objectBox.height / 2.0f);

    // Calculate the error (distance) between the two centers
    cv::Point2f error = objectCenter - frameCenter;

    return error;
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
    float pitchPIDOutput = computePID(filteredError.y, m_pitchKp, m_pitchKi, m_pitchKd,
                                      m_pitchIntegral, m_pitchPreviousError, deltaTime);
    
    // Compute YAW PID output (using horizontal error for yaw control)
    float yawPIDOutput = computePID(filteredError.x, m_yawKp, m_yawKi, m_yawKd,
                                    m_yawIntegral, m_yawPreviousError, deltaTime);

    m_firstUpdate = false;
    
    // Convert PID outputs to RC commands (1000-2000)
    const int neutral = 1547;
    const int max_deviation = 200;  // Reduced from 200 to 100 for smoother response

    uint16_t channels[18] = {0};

    // Apply PID outputs to neutral position
    int roll_output_desired = static_cast<int>(neutral + rollPIDOutput);
    int pitch_output_desired = static_cast<int>(neutral + pitchPIDOutput);
    int yaw_output_desired = static_cast<int>(neutral + yawPIDOutput);
    
    // Clamp the desired values to a safe range
    roll_output_desired = std::clamp(roll_output_desired, neutral - max_deviation, neutral + max_deviation);
    pitch_output_desired = std::clamp(pitch_output_desired, neutral - max_deviation, neutral + max_deviation);
    yaw_output_desired = std::clamp(yaw_output_desired, neutral - max_deviation, neutral + max_deviation);

    // Apply slew rate limiting for smooth transitions
    int roll_output = applySlewRateLimit(roll_output_desired, m_previousRollOutput, deltaTime);
    //int pitch_output = applySlewRateLimit(pitch_output_desired, m_previousPitchOutput, deltaTime);
    int pitch_output = pitch_output_desired;
    int yaw_output = applySlewRateLimit(yaw_output_desired, m_previousYawOutput, deltaTime);
    
    // Update previous outputs for next iteration
    m_previousRollOutput = roll_output;
    m_previousPitchOutput = pitch_output;
    m_previousYawOutput = yaw_output;
    m_firstOutputUpdate = false;

    // Store outputs for debugging
    outputs.roll_output = roll_output;
    outputs.pitch_output = pitch_output;
    outputs.yaw_output = yaw_output;
    
    // Check if frame center is inside the bounding box
    bool centerInBox = isFrameCenterInBoundingBox(objectBox, frameWidth, frameHeight);
    outputs.rc_commands_sent = !centerInBox;  // Only send commands if center is outside box
    
    // Check if object center is within yaw dead zone (vertical tolerance around frame center)
    bool objectInYawDeadZone = isObjectInYawDeadZone(objectBox, frameWidth, frameHeight);

    // Send RC override command only if center is outside bounding box AND MAVLink is available
    if (!centerInBox && m_mavlink) {
        // Create the RC override command

//        channels[0] = roll_output;    // Roll (Channel 1)
        channels[1] = pitch_output;   // Pitch (Channel 2)
        
        // Only send yaw command if object is outside the yaw dead zone
        /*if (!objectInYawDeadZone) {
            channels[3] = yaw_output;     // Yaw (Channel 4)
        }
        else{
            channels[3] = 0;
        }*/

        // Set remaining channels to "ignore"
        /*for (int i = 2; i < 18; ++i) {
            channels[i] = 0;
        }*/
        // channels[3] = 0;
        LOG_DEBUG("Sending RC override - Pitch: {}, Yaw: {}, Center in box: {}, Object in yaw zone: {}", 
                  pitch_output, yaw_output, centerInBox, objectInYawDeadZone);
        // Send the command
        (void)m_mavlink->sendRCOverride(channels);  // Cast to void to suppress warning
    } else if (centerInBox) {
        // Frame center is inside bounding box - object is centered, no RC commands needed
        // PID calculations continue for smooth transitions when object moves away from center
        LOG_INFO("Object centered - Initiating landing sequence");
//        channels[1] = 1527;
        //m_mavlink->land();
        outputs.pitch_output = 4 * std::abs(outputs.pitch_output - neutral) + neutral;
        //outputs.pitch_output = neutral ;
        channels[1] = outputs.pitch_output;
        (void)m_mavlink->sendRCOverride(channels);  // Cast to void to suppress warning
    } else {
        LOG_ERROR("NavigationUnit: MAVLink system not set, cannot send RC override");
    }
    m_lastOutputs = outputs;
    return outputs;
}

bool NavigationUnit::isFrameCenterInBoundingBox(const cv::Rect& objectBox, int frameWidth, int frameHeight) {
    // Calculate the center point of the frame
    cv::Point2f frameCenter(frameWidth / 2.0f, 2.0f * (frameHeight / 3.0f));
    
    // Calculate the center of the tracked object's bounding box
    cv::Point2f objectCenter(objectBox.x + objectBox.width / 2.0f, 
                             objectBox.y + objectBox.height / 2.0f);
    
    // Calculate distance between frame center and object center
    float dx = frameCenter.x - objectCenter.x;
    float dy = frameCenter.y - objectCenter.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
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
