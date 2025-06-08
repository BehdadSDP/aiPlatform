#ifndef RESOURCE_MONITOR_H
#define RESOURCE_MONITOR_H

#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

class ResourceMonitor {
public:
    static ResourceMonitor& getInstance() {
        static ResourceMonitor instance;
        return instance;
    }

    void startMonitoring(const std::string& logFilePath, int intervalSeconds = 60);
    void stopMonitoring();

private:
    ResourceMonitor() : isRunning_(false) {}
    ResourceMonitor(const ResourceMonitor&) = delete;
    ResourceMonitor& operator=(const ResourceMonitor&) = delete;

    void monitorLoop(int intervalSeconds);
    float getCpuUsage();
    float getMemoryUsage();
    float getTemperature();

    std::atomic<bool> isRunning_;
    std::thread monitorThread_;
    std::ofstream logFile_;
    std::mutex mutex_;
};

#endif // RESOURCE_MONITOR_H 