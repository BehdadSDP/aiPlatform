#ifndef APPLICATION_H
#define APPLICATION_H

#include "include/camera_handler.h"
#include "include/video_handler.h"
#include "include/model_manager.h"
#include "include/control_unit.h"
#include "include/tracker_manager.h"
#include "include/visualizer.h"
#include "include/failure_handler.h"
#include "include/mavlink.h"
#include "include/navigation_unit.h"
#include <atomic>
#include <memory>
#include <string>
#include <map>
#include <mutex>

class Application {
public:
    Application();
    ~Application();

    bool initialize(const std::string& configPath);
    void run();

private:
    void setupSignalHandler();
    static void signalHandler(int signum);

    bool loadConfiguration(const std::string& configPath);
    void logConfiguration() const;
    bool initializeInputSource();
    bool initializeModels();
    bool initializeTracker();
    void initializeFrameBuffer();
    bool initializeMAVLink();

    void modelsThread();
    void trackingThread();
    void navigationThread();
    void cleanup();

    static std::atomic<bool> m_running;
    ControlUnit m_controlUnit;
    ModelManager m_modelManager;
    TrackerManager m_trackerManager;
    NavigationUnit m_navigationUnit;
    Visualizer m_visualizer;
    DetectionFailure m_detectionFailure;
    std::mutex m_visMutex;

    std::map<std::string, std::string> m_config;
    std::unique_ptr<CameraHandler> m_cameraHandler;
    std::unique_ptr<VideoHandler> m_videoHandler;
    std::unique_ptr<Mavlink> m_mavlink;

    int m_operationMode;
    bool m_showTrackingPath;
    int m_selectionStrategy;
    bool m_mavlinkEnabled;
    std::string m_sessionFolder;  // Timestamped folder for current session
};

#endif // APPLICATION_H 
