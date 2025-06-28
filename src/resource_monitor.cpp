#include "include/resource_monitor.h"
#include <sstream>
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <ctime>

void ResourceMonitor::startMonitoring(const std::string& logFilePath, int intervalSeconds) {
    if (isRunning_) {
        std::cerr << "Resource monitoring is already running" << std::endl;
        return;
    }
    
    try {
        // Create directory if it doesn't exist
        std::filesystem::path logPath(logFilePath);
        std::filesystem::create_directories(logPath.parent_path());
        
        std::cout << "Opening log file: " << logFilePath << std::endl;
        logFile_.open(logFilePath, std::ios::app);
        
        if (!logFile_.is_open()) {
            throw std::runtime_error("Failed to open log file: " + logFilePath);
        }
        
        std::cout << "Log file opened successfully" << std::endl;

        // Write header if file is empty
        if (logFile_.tellp() == 0) {
            logFile_ << "Timestamp,CPU Usage (%),Memory Usage (MB),Temperature (°C)\n";
            logFile_.flush();
            std::cout << "Header written to log file" << std::endl;
        }

        isRunning_ = true;
        monitorThread_ = std::thread(&ResourceMonitor::monitorLoop, this, intervalSeconds);
        std::cout << "Resource monitoring started with interval: " << intervalSeconds << " seconds" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error starting resource monitoring: " << e.what() << std::endl;
        throw;
    }
}

void ResourceMonitor::stopMonitoring() {
    std::cout << "Stopping resource monitoring..." << std::endl;
    isRunning_ = false;
    
    if (monitorThread_.joinable()) {
        std::cout << "Waiting for monitor thread to finish..." << std::endl;
        monitorThread_.join();
    }
    
    if (logFile_.is_open()) {
        std::cout << "Closing log file..." << std::endl;
        logFile_.close();
    }
    
    std::cout << "Resource monitoring stopped" << std::endl;
}

void ResourceMonitor::monitorLoop(int intervalSeconds) {
    std::cout << "Monitor loop started" << std::endl;
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
                std::cerr << "Failed to get local time" << std::endl;
                continue;
            }
            
            std::stringstream timestamp;
            timestamp << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
            
            float cpuUsage = getCpuUsage();
            float memoryUsage = getMemoryUsage();
            float temperature = getTemperature();

            std::lock_guard<std::mutex> lock(mutex_);
            if (!logFile_.is_open()) {
                std::cerr << "Log file is not open" << std::endl;
                continue;
            }
            
            // Write header if file is empty
            if (logFile_.tellp() == 0) {
                logFile_ << "Timestamp,CPU Usage (%),Memory Usage (MB),Temperature (°C)\n";
                logFile_.flush();
                std::cout << "Header written to log file" << std::endl;
            }
            
            logFile_ << timestamp.str() << ","
                    << std::fixed << std::setprecision(2) << cpuUsage << ","
                    << std::fixed << std::setprecision(2) << memoryUsage << ","
                    << std::fixed << std::setprecision(2) << temperature << "\n";
            logFile_.flush();
            
            std::cout << "Logged metrics at " << timestamp.str() 
                      << " - CPU: " << std::fixed << std::setprecision(2) << cpuUsage 
                      << "%, Memory: " << std::fixed << std::setprecision(2) << memoryUsage 
                      << "MB, Temp: " << std::fixed << std::setprecision(2) << temperature 
                      << "°C" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Resource monitoring error: " << e.what() << std::endl;
        }
    }
    std::cout << "Monitor loop ended" << std::endl;
}

float ResourceMonitor::getCpuUsage() {
    std::ifstream statFile("/proc/stat");
    if (!statFile.is_open()) {
        std::cerr << "Failed to open /proc/stat" << std::endl;
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

float ResourceMonitor::getMemoryUsage() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        std::cerr << "Failed to open /proc/meminfo" << std::endl;
        return 0.0f;
    }
    
    std::string line;
    unsigned long totalMem = 0, freeMem = 0, buffers = 0, cached = 0;
    
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") != std::string::npos) {
            sscanf(line.c_str(), "MemTotal: %lu", &totalMem);
        } else if (line.find("MemFree:") != std::string::npos) {
            sscanf(line.c_str(), "MemFree: %lu", &freeMem);
        } else if (line.find("Buffers:") != std::string::npos) {
            sscanf(line.c_str(), "Buffers: %lu", &buffers);
        } else if (line.find("Cached:") != std::string::npos) {
            sscanf(line.c_str(), "Cached: %lu", &cached);
        }
    }
    
    unsigned long usedMem = totalMem - freeMem - buffers - cached;
    return usedMem / 1024.0f; // Convert to MB
}

float ResourceMonitor::getTemperature() {
    std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
    if (!tempFile.is_open()) {
        std::cerr << "Failed to open temperature file" << std::endl;
        return 0.0f;
    }
    float temp;
    tempFile >> temp;
    return temp / 1000.0f;  // Convert from millicelsius to celsius
} 