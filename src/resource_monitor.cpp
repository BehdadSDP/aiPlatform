#include "include/resource_monitor.h"
#include "include/logger.h"
#include <sstream>
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <ctime>

void ResourceMonitor::startMonitoring(const std::string& logFilePath, int intervalSeconds) {
    if (isRunning_) {
        LOG_WARN("Resource monitoring is already running");
        return;
    }
    
    try {
        // Create directory if it doesn't exist
        std::filesystem::path logPath(logFilePath);
        std::filesystem::create_directories(logPath.parent_path());
        
        LOG_DEBUG("Opening log file: {}", logFilePath);
        logFile_.open(logFilePath, std::ios::app);
        
        if (!logFile_.is_open()) {
            throw std::runtime_error("Failed to open log file: " + logFilePath);
        }
        
        LOG_DEBUG("Log file opened successfully");

        // Write header if file is empty
        if (logFile_.tellp() == 0) {
            logFile_ << "Timestamp,CPU Usage (%),Total Memory (MB),Used Memory (MB),Buffers (MB),Shared (MB),Cache (MB),Available (MB),Temperature (°C)\n";
            logFile_.flush();
            LOG_DEBUG("Header written to log file");
        }

        isRunning_ = true;
        monitorThread_ = std::thread(&ResourceMonitor::monitorLoop, this, intervalSeconds);
        LOG_INFO("Resource monitoring started with interval: {} seconds", intervalSeconds);
    } catch (const std::exception& e) {
        LOG_ERROR("Error starting resource monitoring: {}", e.what());
        throw;
    }
}

void ResourceMonitor::stopMonitoring() {
    LOG_DEBUG("Stopping resource monitoring...");
    isRunning_ = false;
    
    if (monitorThread_.joinable()) {
        LOG_DEBUG("Waiting for monitor thread to finish...");
        monitorThread_.join();
    }
    
    if (logFile_.is_open()) {
        LOG_DEBUG("Closing log file...");
        logFile_.close();
    }
    
    LOG_INFO("Resource monitoring stopped");
}

void ResourceMonitor::monitorLoop(int intervalSeconds) {
    LOG_DEBUG("Monitor loop started");
    while (isRunning_) {
        try {
            std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
            
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            
            // Use thread-safe time functions
            std::tm tm_buf;
            std::tm* tm = nullptr;
            
            #ifdef _WIN32
                localtime_s(&tm_buf, &time);
                tm = &tm_buf;
            #else
                // Use localtime_r for thread safety on Unix systems
                tm = localtime_r(&time, &tm_buf);
            #endif
            
            if (!tm) {
                LOG_ERROR("Failed to get local time");
                continue;
            }
            
            std::stringstream timestamp;
            timestamp << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
            
            float cpuUsage = getCpuUsage();
            float temperature = getTemperature();

            std::lock_guard<std::mutex> lock(mutex_);
            if (!logFile_.is_open()) {
                LOG_ERROR("Log file is not open");
                continue;
            }
            
            // Write header if file is empty
            if (logFile_.tellp() == 0) {
                logFile_ << "Timestamp,CPU Usage (%),Total Memory (MB),Used Memory (MB),Buffers (MB),Shared (MB),Cache (MB),Available (MB),Temperature (°C)\n";
                logFile_.flush();
                LOG_DEBUG("Header written to log file");
            }
            
            MemoryUsage memUsage = getDetailedMemoryUsage();
            
            logFile_ << timestamp.str() << ","
                    << std::fixed << std::setprecision(2) << cpuUsage << ","
                    << std::fixed << std::setprecision(2) << memUsage.total << ","
                    << std::fixed << std::setprecision(2) << memUsage.used << ","
                    << std::fixed << std::setprecision(2) << memUsage.buffers << ","
                    << std::fixed << std::setprecision(2) << memUsage.shared << ","
                    << std::fixed << std::setprecision(2) << memUsage.cache << ","
                    << std::fixed << std::setprecision(2) << memUsage.available << ","
                    << std::fixed << std::setprecision(2) << temperature << "\n";
            logFile_.flush();
            
            LOG_DEBUG("Logged metrics at {} - CPU: {:.2f}%, Memory: {:.2f}/{:.2f}MB (Used/Total), Temp: {:.2f}°C",
                      timestamp.str(), cpuUsage, memUsage.used, memUsage.total, temperature);
        
            // Detailed memory breakdown (every 10th log to avoid spam)
            static int logCounter = 0;
            if (++logCounter % 10 == 0) {
                LOG_DEBUG("  Memory Details - Used: {:.2f}MB, Buffers: {:.2f}MB, Cache: {:.2f}MB, Available: {:.2f}MB",
                          memUsage.used, memUsage.buffers, memUsage.cache, memUsage.available);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Resource monitoring error: {}", e.what());
        }
    }
    LOG_DEBUG("Monitor loop ended");
}

float ResourceMonitor::getCpuUsage() {
    std::ifstream statFile("/proc/stat");
    if (!statFile.is_open()) {
        LOG_ERROR("Failed to open /proc/stat");
        return 0.0f;
    }
    
    std::string line;
    std::getline(statFile, line);
    statFile.close();

    std::istringstream iss(line);
    std::string cpu;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;

    unsigned long long currentIdleTime = idle + iowait;
    unsigned long long currentTotalTime = user + nice + system + currentIdleTime + irq + softirq + steal;

    if (prevTotalTime_ == 0) {
        // First run, just store values and return 0
        prevTotalTime_ = currentTotalTime;
        prevIdleTime_ = currentIdleTime;
        return 0.0f;
    }

    unsigned long long totalTimeDiff = currentTotalTime - prevTotalTime_;
    unsigned long long idleTimeDiff = currentIdleTime - prevIdleTime_;
    
    prevTotalTime_ = currentTotalTime;
    prevIdleTime_ = currentIdleTime;

    if (totalTimeDiff == 0) {
        return 0.0f;
    }

    float cpu_usage = 100.0 * (1.0 - (double)idleTimeDiff / totalTimeDiff);
    return cpu_usage;
}

MemoryUsage ResourceMonitor::getDetailedMemoryUsage() {
    MemoryUsage memUsage;
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        LOG_ERROR("Failed to open /proc/meminfo");
        return memUsage;
    }
    
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") != std::string::npos) {
            unsigned long total;
            sscanf(line.c_str(), "MemTotal: %lu", &total);
            memUsage.total = total / 1024.0f; // Convert KB to MB
        } else if (line.find("MemFree:") != std::string::npos) {
            unsigned long free;
            sscanf(line.c_str(), "MemFree: %lu", &free);
            memUsage.free = free / 1024.0f; // Convert KB to MB
        } else if (line.find("Buffers:") != std::string::npos) {
            unsigned long buffers;
            sscanf(line.c_str(), "Buffers: %lu", &buffers);
            memUsage.buffers = buffers / 1024.0f; // Convert KB to MB
        } else if (line.find("Cached:") != std::string::npos) {
            unsigned long cached;
            sscanf(line.c_str(), "Cached: %lu", &cached);
            memUsage.cache = cached / 1024.0f; // Convert KB to MB
        } else if (line.find("SwapTotal:") != std::string::npos) {
            unsigned long swapTotal;
            sscanf(line.c_str(), "SwapTotal: %lu", &swapTotal);
            memUsage.swapTotal = swapTotal / 1024.0f; // Convert KB to MB
        } else if (line.find("SwapFree:") != std::string::npos) {
            unsigned long swapFree;
            sscanf(line.c_str(), "SwapFree: %lu", &swapFree);
            memUsage.swapFree = swapFree / 1024.0f; // Convert KB to MB
        }
    }
    
    // Calculate derived values
    memUsage.used = memUsage.total - memUsage.free - memUsage.buffers - memUsage.cache;
    memUsage.available = memUsage.free + memUsage.buffers + memUsage.cache;
    memUsage.shared = 0; // Shared memory is not directly available in /proc/meminfo
    memUsage.swapUsed = memUsage.swapTotal - memUsage.swapFree;
    memUsage.swapAvailable = memUsage.swapFree;
    
    return memUsage;
}

float ResourceMonitor::getMemoryUsage() {
    // Legacy method for backward compatibility
    MemoryUsage memUsage = getDetailedMemoryUsage();
    return memUsage.used;
}

float ResourceMonitor::getTemperature() {
    std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
    if (!tempFile.is_open()) {
        LOG_ERROR("Failed to open temperature file");
        return 0.0f;
    }
    float temp;
    tempFile >> temp;
    return temp / 1000.0f;  // Convert from millicelsius to celsius
} 