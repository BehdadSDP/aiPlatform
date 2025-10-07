#ifndef NAVIGATION_UNIT_H
#define NAVIGATION_UNIT_H

#include <opencv2/core/types.hpp>
#include <chrono>

// Forward declaration to avoid including the full mavlink.h
class Mavlink;

struct ControlOutputs {
    cv::Point2f error;
    int roll_output;
    int pitch_output;
    bool rc_commands_sent;  // Indicates if RC override commands were actually sent
};

class NavigationUnit {
public:
    NavigationUnit();

    /**
     * @brief Sets the MAVLink instance to be used for sending commands.
     * @param mavlink A pointer to the Mavlink object.
     */
    void setMavlink(Mavlink* mavlink);

    /**
     * @brief Calculates the error between the center of the frame and the center of the tracked object.
     * 
     * @param objectBox The bounding box of the tracked object.
     * @param frameWidth The width of the camera frame.
     * @param frameHeight The height of the camera frame.
     * @return A cv::Point2f where x is the horizontal error and y is the vertical error.
     *         Positive x means the object is to the right of center.
     *         Positive y means the object is below the center.
     */
    cv::Point2f calculateError(const cv::Rect& objectBox, int frameWidth, int frameHeight);

    /**
     * @brief Generates and sends manual control commands based on the tracking error using PID controller.
     * @param error The tracking error (dx, dy) from the center of the frame.
     * @return ControlOutputs structure containing error and output values.
     */
    ControlOutputs generateControlCommands(const cv::Point2f& error);

    /**
     * @brief Generates and sends manual control commands with bounding box check.
     * @param error The tracking error (dx, dy) from the center of the frame.
     * @param objectBox The bounding box of the tracked object.
     * @param frameWidth The width of the camera frame.
     * @param frameHeight The height of the camera frame.
     * @return ControlOutputs structure containing error and output values.
     */
    ControlOutputs generateControlCommands(const cv::Point2f& error, const cv::Rect& objectBox, int frameWidth, int frameHeight);

    /**
     * @brief Checks if the center of the frame is inside the tracked object's bounding box.
     * @param objectBox The bounding box of the tracked object.
     * @param frameWidth The width of the camera frame.
     * @param frameHeight The height of the camera frame.
     * @return True if frame center is inside the bounding box, false otherwise.
     */
    bool isFrameCenterInBoundingBox(const cv::Rect& objectBox, int frameWidth, int frameHeight);

    /**
     * @brief Arms or disarms the vehicle.
     * @param arm True to arm, false to disarm.
     * @param force Force arm/disarm even if pre-arm checks fail.
     * @return True if command was sent successfully.
     */
    bool armVehicle(bool arm, bool force = false);

    /**
     * @brief Sets PID gains for roll axis.
     * @param kp Proportional gain
     * @param ki Integral gain  
     * @param kd Derivative gain
     */
    void setRollPIDGains(float kp, float ki, float kd);

    /**
     * @brief Sets PID gains for pitch axis.
     * @param kp Proportional gain
     * @param ki Integral gain
     * @param kd Derivative gain
     */
    void setPitchPIDGains(float kp, float ki, float kd);

    /**
     * @brief Resets PID controller states (integral and derivative terms).
     */
    void resetPIDStates();

    /**
     * @brief Gets the last computed control outputs for debugging/visualization.
     * @return Reference to the last ControlOutputs structure.
     */
    const ControlOutputs& getLastControlOutputs() const;

    /**
     * @brief Sets the low-pass filter smoothing factor (alpha).
     * @param alpha Smoothing factor between 0.0 and 1.0. 
     *              Lower values = more smoothing, higher values = less smoothing.
     */
    void setFilterAlpha(float alpha);

private:
    Mavlink* m_mavlink = nullptr;
    
    // PID gains for roll axis
    float m_rollKp = 0.5f;
    float m_rollKi = 0.1f;
    float m_rollKd = 0.05f;
    
    // PID gains for pitch axis
    float m_pitchKp = 0.5f;
    float m_pitchKi = 0.1f;
    float m_pitchKd = 0.05f;
    
    // PID state variables for roll
    float m_rollIntegral = 0.0f;
    float m_rollPreviousError = 0.0f;
    
    // PID state variables for pitch
    float m_pitchIntegral = 0.0f;
    float m_pitchPreviousError = 0.0f;
    
    // Timing for derivative calculation
    std::chrono::steady_clock::time_point m_lastUpdateTime;
    bool m_firstUpdate = true;
    
    // Integral windup protection
    float m_integralMax = 100.0f;
    
    // Low-pass filter for error smoothing (Exponential Moving Average)
    float m_filterAlpha = 0.3f;  // Smoothing factor (0.0 = max smoothing, 1.0 = no smoothing)
    cv::Point2f m_filteredError = cv::Point2f(0.0f, 0.0f);  // Previous filtered error
    bool m_firstFilterUpdate = true;  // Flag for first filter update
    
    // Store last outputs for debugging
    ControlOutputs m_lastOutputs;

    /**
     * @brief Computes PID output for a single axis.
     * @param error Current error value
     * @param kp Proportional gain
     * @param ki Integral gain
     * @param kd Derivative gain
     * @param integral Reference to integral accumulator
     * @param previousError Reference to previous error storage
     * @param deltaTime Time since last update in seconds
     * @return PID controller output
     */
    float computePID(float error, float kp, float ki, float kd, 
                     float& integral, float& previousError, float deltaTime);

    /**
     * @brief Applies low-pass filter (Exponential Moving Average) to smooth error values.
     * @param rawError The raw, unfiltered error value
     * @return Filtered error value
     */
    cv::Point2f applyLowPassFilter(const cv::Point2f& rawError);
};

#endif // NAVIGATION_UNIT_H
