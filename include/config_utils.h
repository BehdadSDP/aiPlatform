#ifndef CONFIG_UTILS_H
#define CONFIG_UTILS_H

#include <map>
#include <string>
#include <vector>

namespace config_utils {
// Trim whitespace from a string
std::string trim(const std::string& str);

// Load configuration from a file
std::map<std::string, std::string> loadConfig(const std::string& filename);

// Get an integer value from the config map
int getConfigInt(const std::map<std::string, std::string>& config, const std::string& key);

// Get a float value from the config map
float getConfigFloat(const std::map<std::string, std::string>& config, const std::string& key);

// Get a string value from the config map
std::string getConfigString(const std::map<std::string, std::string>& config, const std::string& key);

} // namespace config_utils

#endif // CONFIG_UTILS_H
