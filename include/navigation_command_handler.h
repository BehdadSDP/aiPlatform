#ifndef NAVIGATION_COMMAND_HANDLER_H
#define NAVIGATION_COMMAND_HANDLER_H

#include <opencv2/core/types.hpp>
#include <chrono>
#include <functional>
#include <string>

// Forward declarations
class Mavlink;

/**
 * @brief Enumeration of navigation command types
 */
enum class NavCommandType {
    NONE = 0,
    MOVE_TO_POSITION,      // Move to specific GPS coordinates
    MOVE_RELATIVE,         // Move relative to current position (NED frame)
    ROTATE_TO_HEADING,     // Rotate to specific heading
    CIRCLE_TARGET,         // Circle around a target point
    ORBIT_TARGET,          // Orbit around tracked object
    APPROACH_TARGET,       // Gradually approach tracked object
    MAINTAIN_DISTANCE,     // Maintain fixed distance from target
    SPIRAL_SEARCH,         // Spiral search pattern
    HOVER,                 // Hold current position
    RETURN_HOME,           // Return to home position
    EMERGENCY_STOP         // Emergency stop all movement
};

/**
 * @brief Status of command execution
 */
enum class NavCommandStatus {
    IDLE,              // No command active
    INITIALIZING,      // Command starting
    IN_PROGRESS,       // Command executing
    COMPLETED,         // Command finished successfully
    FAILED,            // Command failed
    ABORTED            // Command cancelled
};

/**
 * @brief Structure for navigation command parameters
 */
struct NavCommandParams {
    NavCommandType type = NavCommandType::NONE;
    
    // Position parameters (GPS or relative)
    double latitude = 0.0;
    double longitude = 0.0;
    float altitude = 0.0f;
    
    // Relative movement parameters (NED frame: North, East, Down in meters)
    float north = 0.0f;
    float east = 0.0f;
    float down = 0.0f;
    
    // Rotation parameters
    float target_heading = 0.0f;    // Target heading in degrees (0-360)
    float rotation_rate = 0.0f;     // Rotation rate in deg/sec
    
    // Circle/Orbit parameters
    float radius = 0.0f;            // Circle radius in meters
    float angular_velocity = 0.0f;  // Angular velocity in deg/sec
    bool clockwise = true;          // Orbit direction
    
    // Approach parameters
    float target_distance = 0.0f;   // Target distance from object in meters
    float approach_speed = 0.0f;    // Approach speed in m/s
    
    // Search parameters
    float search_radius = 0.0f;     // Search area radius
    float search_altitude = 0.0f;   // Search altitude
    
    // Timing parameters
    float duration = 0.0f;          // Command duration in seconds (0 = indefinite)
    float timeout = 0.0f;           // Command timeout in seconds
    
    // Callback for command completion
    std::function<void(NavCommandStatus)> completion_callback = nullptr;
};

/**
 * @brief Result structure for command execution
 */
struct NavCommandResult {
    NavCommandStatus status;
    float elapsed_time;         // Time elapsed since command start
    float progress;             // Progress percentage (0.0-1.0)
    cv::Point2f current_error;  // Current tracking error
    std::string message;        // Status message
};

/**
 * @brief Navigation Command Handler - Manages high-level navigation commands
 * 
 * This class provides an interface for executing complex navigation behaviors
 * beyond simple PID tracking. It works alongside NavigationUnit to provide
 * autonomous flight patterns and maneuvers.
 */
class NavigationCommandHandler {
public:
    NavigationCommandHandler();
    ~NavigationCommandHandler();

    /**
     * @brief Sets the MAVLink instance for sending commands
     * @param mavlink Pointer to the Mavlink object
     */
    void setMavlink(Mavlink* mavlink);

    /**
     * @brief Execute a navigation command
     * @param params Command parameters structure
     * @return true if command accepted and started, false otherwise
     */
    bool executeCommand(const NavCommandParams& params);

    /**
     * @brief Update command execution (should be called periodically)
     * @return Current command result
     */
    NavCommandResult update();

    /**
     * @brief Abort the current command
     * @return true if command aborted successfully
     */
    bool abortCommand();

    /**
     * @brief Check if a command is currently active
     * @return true if command is executing
     */
    bool isCommandActive() const;

    /**
     * @brief Get the current command status
     * @return Current NavCommandStatus
     */
    NavCommandStatus getCommandStatus() const;

    /**
     * @brief Get the current command type
     * @return Current NavCommandType
     */
    NavCommandType getCurrentCommandType() const;

    /**
     * @brief Emergency stop - immediately halt all movement
     * @return true if emergency stop executed
     */
    bool emergencyStop();

    // === CONVENIENCE METHODS FOR COMMON COMMANDS ===

    /**
     * @brief Move to a specific GPS position
     * @param latitude Target latitude
     * @param longitude Target longitude
     * @param altitude Target altitude in meters
     * @param timeout Command timeout in seconds
     * @return true if command accepted
     */
    bool moveToPosition(double latitude, double longitude, float altitude, float timeout = 30.0f);

    /**
     * @brief Move relative to current position (NED frame)
     * @param north Distance north in meters
     * @param east Distance east in meters
     * @param down Distance down in meters (negative = up)
     * @param timeout Command timeout in seconds
     * @return true if command accepted
     */
    bool moveRelative(float north, float east, float down, float timeout = 30.0f);

    /**
     * @brief Rotate to a specific heading
     * @param heading Target heading in degrees (0-360)
     * @param rate Rotation rate in deg/sec (default: 30)
     * @return true if command accepted
     */
    bool rotateToHeading(float heading, float rate = 30.0f);

    /**
     * @brief Circle around a GPS point
     * @param latitude Center latitude
     * @param longitude Center longitude
     * @param radius Circle radius in meters
     * @param altitude Circle altitude in meters
     * @param angular_velocity Angular velocity in deg/sec (default: 10)
     * @param clockwise Circle direction (default: true)
     * @param duration Circle duration in seconds (0 = indefinite)
     * @return true if command accepted
     */
    bool circlePosition(double latitude, double longitude, float radius, float altitude,
                       float angular_velocity = 10.0f, bool clockwise = true, float duration = 0.0f);

    /**
     * @brief Orbit around the currently tracked object
     * @param radius Orbit radius in meters
     * @param angular_velocity Angular velocity in deg/sec
     * @param clockwise Orbit direction (default: true)
     * @param duration Orbit duration in seconds (0 = indefinite)
     * @return true if command accepted
     */
    bool orbitTrackedObject(float radius, float angular_velocity = 10.0f, 
                           bool clockwise = true, float duration = 0.0f);

    /**
     * @brief Gradually approach the tracked object
     * @param target_distance Target distance in meters
     * @param approach_speed Approach speed in m/s
     * @return true if command accepted
     */
    bool approachTrackedObject(float target_distance, float approach_speed = 0.5f);

    /**
     * @brief Maintain a fixed distance from tracked object
     * @param distance Distance to maintain in meters
     * @return true if command accepted
     */
    bool maintainDistance(float distance);

    /**
     * @brief Execute a spiral search pattern
     * @param center_latitude Search center latitude
     * @param center_longitude Search center longitude
     * @param max_radius Maximum search radius in meters
     * @param altitude Search altitude in meters
     * @return true if command accepted
     */
    bool spiralSearch(double center_latitude, double center_longitude, 
                     float max_radius, float altitude);

    /**
     * @brief Hover at current position
     * @param duration Hover duration in seconds (0 = indefinite)
     * @return true if command accepted
     */
    bool hover(float duration = 0.0f);

    /**
     * @brief Return to home position
     * @return true if command accepted
     */
    bool returnHome();

private:
    Mavlink* m_mavlink;
    
    // Current command state
    NavCommandParams m_currentCommand;
    NavCommandStatus m_commandStatus;
    std::chrono::steady_clock::time_point m_commandStartTime;
    
    // Command execution state
    float m_commandProgress;
    cv::Point2f m_lastTrackingError;
    
    // Private execution methods for each command type
    NavCommandResult executeMoveToPosition();
    NavCommandResult executeMoveRelative();
    NavCommandResult executeRotateToHeading();
    NavCommandResult executeCircleTarget();
    NavCommandResult executeOrbitTarget();
    NavCommandResult executeApproachTarget();
    NavCommandResult executeMaintainDistance();
    NavCommandResult executeSpiralSearch();
    NavCommandResult executeHover();
    NavCommandResult executeReturnHome();
    
    // Helper methods
    float getElapsedTime() const;
    bool isTimeoutExpired() const;
    void resetCommandState();
    void invokeCompletionCallback(NavCommandStatus status);
    
    /**
     * @brief Send a position target in local NED frame
     * @param x North position in meters
     * @param y East position in meters
     * @param z Down position in meters (negative = up)
     * @param vx North velocity in m/s
     * @param vy East velocity in m/s
     * @param vz Down velocity in m/s
     * @param yaw Yaw angle in radians
     * @param yaw_rate Yaw rate in rad/s
     * @return true if sent successfully
     */
    bool sendPositionTargetLocalNED(float x, float y, float z, 
                                   float vx, float vy, float vz,
                                   float yaw, float yaw_rate);
    
    /**
     * @brief Send a position target in global GPS frame
     * @param lat_int Latitude in degrees * 1e7
     * @param lon_int Longitude in degrees * 1e7
     * @param alt Altitude in meters (AMSL)
     * @param vx North velocity in m/s
     * @param vy East velocity in m/s
     * @param vz Down velocity in m/s
     * @param yaw Yaw angle in radians
     * @param yaw_rate Yaw rate in rad/s
     * @return true if sent successfully
     */
    bool sendPositionTargetGlobalInt(int32_t lat_int, int32_t lon_int, float alt,
                                    float vx, float vy, float vz,
                                    float yaw, float yaw_rate);
};

#endif // NAVIGATION_COMMAND_HANDLER_H
