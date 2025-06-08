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
            std::tm* tm = std::localtime(&time);
            
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
    static unsigned long long lastTotalUser = 0, lastTotalUserLow = 0, lastTotalSys = 0, lastTotalIdle = 0;
    
    std::ifstream statFile("/proc/stat");
    if (!statFile.is_open()) {
        std::cerr << "Failed to open /proc/stat" << std::endl;
        return 0.0f;
    }
    
    std::string line;
    std::getline(statFile, line);
    
    unsigned long long totalUser, totalUserLow, totalSys, totalIdle;
    std::istringstream iss(line);
    std::string cpu;
    iss >> cpu >> totalUser >> totalUserLow >> totalSys >> totalIdle;
    
    if (lastTotalUser == 0) {
        lastTotalUser = totalUser;
        lastTotalUserLow = totalUserLow;
        lastTotalSys = totalSys;
        lastTotalIdle = totalIdle;
        return 0.0f;
    }
    
    unsigned long long total = (totalUser - lastTotalUser) +
                             (totalUserLow - lastTotalUserLow) +
                             (totalSys - lastTotalSys);
    unsigned long long idle = totalIdle - lastTotalIdle;
    
    lastTotalUser = totalUser;
    lastTotalUserLow = totalUserLow;
    lastTotalSys = totalSys;
    lastTotalIdle = totalIdle;
    
    return total > 0 ? (float)(total - idle) / total * 100.0f : 0.0f;
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