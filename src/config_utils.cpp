#include "include/config_utils.h"
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace config_utils {

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, last - first + 1);
}

std::map<std::string, std::string> loadConfig(const std::string& filename) {
    std::map<std::string, std::string> config;
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Cannot open config file: " + filename);
    }

    std::string line;
    std::string current_section;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            current_section = trim(line.substr(1, line.size() - 2));
            if (!current_section.empty()) {
                current_section += ".";
            }
            continue;
        }

        const size_t delimiter_pos = line.find('=');
        if (delimiter_pos == std::string::npos) {
            continue;
        }
        std::string key = trim(line.substr(0, delimiter_pos));
        std::string value = trim(line.substr(delimiter_pos + 1));
        if (key.empty()) {
            continue;
        }

        std::string full_key = current_section + key;
        if (config.find(full_key) != config.end()) {
            throw std::runtime_error("Duplicate config key: " + full_key);
        }
        config[full_key] = value;
    }

    const std::vector<std::string> required_keys = {
        "input.input_type",
        "camera.resolution_index",
        "camera.width",
        "camera.height",
        "camera.frame_rate",
        "detection.mode",
        "detection.interval",
        "detection.selection_strategy",
        "tracking.mode",
        "tracking.interval",
        "general.target_class_id",
        "general.operation_mode",
        "detection_model.model_type"
    };
    for (const auto& key : required_keys) {
        if (config.find(key) == config.end()) {
            throw std::runtime_error("Missing required config key: " + key);
        }
    }
    
    // Additional validation for video input
    if (config.find("input.input_type") != config.end()) {
        int inputType = std::stoi(config.at("input.input_type"));
        if (inputType == 1 && config.find("input.video_path") == config.end()) {
            throw std::runtime_error("Missing required config key for video input: input.video_path");
        }
    }

    return config;
}

int getConfigInt(const std::map<std::string, std::string>& config, const std::string& key) {
    auto it = config.find(key);
    if (it == config.end()) {
        throw std::runtime_error("Config key missing: " + key);
    }
    try {
        int value = std::stoi(it->second);
        std::cout << "Config " << key << " = " << value << std::endl; // Optional: keep for debugging
        return value;
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid value for " + key + ": " + it->second);
    }
}

float getConfigFloat(const std::map<std::string, std::string>& config, const std::string& key) {
    auto it = config.find(key);
    if (it == config.end()) {
        throw std::runtime_error("Config key missing: " + key);
    }
    try {
        float value = std::stof(it->second);
        std::cout << "Config " << key << " = " << value << std::endl; // Optional: keep for debugging
        return value;
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid value for " + key + ": " + it->second);
    }
}

std::string getConfigString(const std::map<std::string, std::string>& config, const std::string& key) {
    auto it = config.find(key);
    if (it == config.end()) {
        throw std::runtime_error("Config key missing: " + key);
    }
    std::cout << "Config " << key << " = " << it->second << std::endl; // Optional: keep for debugging
    return it->second; // No conversion needed for string
}

} // namespace config_utils
