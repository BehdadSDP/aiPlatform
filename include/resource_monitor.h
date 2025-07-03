#ifndef RESOURCE_MONITOR_H
#define RESOURCE_MONITOR_H

#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

// Memory usage structure for detailed memory components
struct MemoryUsage {
    float used;      // Used memory in MB
    float buffers;   // Buffer memory in MB
    float shared;    // Shared memory in MB (calculated)
    float cache;     // Cache memory in MB
    float total;     // Total memory in MB
    float available; // Available memory in MB
    float free;      // Free memory in MB
    float swapTotal; // Total swap in MB
    float swapUsed;  // Used swap in MB
    float swapFree;  // Free swap in MB
    float swapAvailable; // Available swap in MB
    
    MemoryUsage() : used(0), buffers(0), shared(0), cache(0), total(0), available(0), 
                    free(0), swapTotal(0), swapUsed(0), swapFree(0), swapAvailable(0) {}
};

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
    MemoryUsage getDetailedMemoryUsage();
    float getMemoryUsage(); // Legacy method for backward compatibility
    float getTemperature();

    std::atomic<bool> isRunning_;
    std::thread monitorThread_;
    std::ofstream logFile_;
    std::mutex mutex_;

    // For CPU Usage Calculation
    unsigned long long prevTotalTime_ = 0;
    unsigned long long prevIdleTime_ = 0;
};

#endif // RESOURCE_MONITOR_H 