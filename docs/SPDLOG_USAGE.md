# spdlog Integration Guide

## Overview
The project now uses **spdlog** for professional logging with the following features:
- Multiple log levels (trace, debug, info, warn, error, critical)
- Colored console output
- Automatic file rotation (5MB per file, 3 files max)
- Thread-safe logging
- Printf-style formatting

## Log Files Location
Logs are saved to: `logs/aiplatform.log`

Rotating files:
- `logs/aiplatform.log` (current)
- `logs/aiplatform.1.log` (previous)
- `logs/aiplatform.2.log` (oldest)

## Usage Examples

### Basic Logging
```cpp
#include "include/logger.h"

// Simple messages
LOG_TRACE("This is a trace message");
LOG_DEBUG("This is a debug message");
LOG_INFO("This is an info message");
LOG_WARN("This is a warning message");
LOG_ERROR("This is an error message");
LOG_CRITICAL("This is a critical message");
```

### Formatted Logging (printf-style)
```cpp
// With variables
int value = 42;
std::string name = "drone";
LOG_INFO("Processing {} with value: {}", name, value);

// Multiple parameters
float x = 10.5, y = 20.3;
LOG_DEBUG("Position: x={:.2f}, y={:.2f}", x, y);

// Complex formatting
LOG_INFO("Frame {}/{} processed in {:.3f}ms", current, total, elapsed);
```

### Logging with File/Line Information
```cpp
// For debugging - includes file and line number
LOG_DEBUG_LOC("Entering function");
LOG_ERROR_LOC("Failed to initialize: {}", error_msg);
```

### Conditional Logging
```cpp
if (some_expensive_check()) {
    LOG_DEBUG("Expensive debug info: {}", compute_debug_string());
}
```

## Log Levels

| Level | When to Use | Example |
|-------|-------------|---------|
| **TRACE** | Very detailed debugging | Function entry/exit, loop iterations |
| **DEBUG** | Debug information | Variable values, state changes |
| **INFO** | Important events | System startup, config loaded, mode changes |
| **WARN** | Warnings | Deprecated usage, recoverable errors |
| **ERROR** | Errors | Failed operations, caught exceptions |
| **CRITICAL** | Fatal errors | System crash, unrecoverable errors |

## Changing Log Level

### At Runtime (in main.cpp)
```cpp
// Show everything (very verbose)
Logger::setLevel(spdlog::level::trace);

// Show debug and above
Logger::setLevel(spdlog::level::debug);

// Default: Show info and above
Logger::setLevel(spdlog::level::info);

// Production: Show warnings and above
Logger::setLevel(spdlog::level::warn);

// Critical only
Logger::setLevel(spdlog::level::critical);
```

### Example: Debug Mode
```cpp
// In main.cpp
#ifdef DEBUG_BUILD
    Logger::setLevel(spdlog::level::debug);
#else
    Logger::setLevel(spdlog::level::info);
#endif
```

## Best Practices

### ✅ DO:
```cpp
// Use appropriate log levels
LOG_INFO("System started successfully");
LOG_ERROR("Failed to connect to camera: {}", error);

// Log important state changes
LOG_INFO("Switching to flight mode: ALT_HOLD");

// Log errors with context
LOG_ERROR("Failed to process frame {}: {}", frame_id, error_msg);

// Use formatting for readability
LOG_DEBUG("Tracking object at ({}, {}) with confidence {:.2f}", x, y, conf);
```

### ❌ DON'T:
```cpp
// Don't log in tight loops without guards
for (int i = 0; i < 1000000; i++) {
    LOG_DEBUG("Processing {}");  // BAD: Too much output
}

// Don't use wrong log levels
LOG_CRITICAL("Button clicked");  // BAD: Not critical
LOG_INFO("Segmentation fault occurred");  // BAD: Should be ERROR or CRITICAL

// Don't compute expensive strings unnecessarily
LOG_DEBUG("Data: " + expensive_computation());  // BAD: Computed even if DEBUG disabled
// Instead:
if (Logger::getLogger()->should_log(spdlog::level::debug)) {
    LOG_DEBUG("Data: {}", expensive_computation());
}
```

## Migration from std::cout/std::cerr

### Before:
```cpp
std::cout << "System started" << std::endl;
std::cerr << "ERROR: Failed to initialize: " << error << std::endl;
```

### After:
```cpp
LOG_INFO("System started");
LOG_ERROR("Failed to initialize: {}", error);
```

## Performance Tips

1. **Use appropriate log levels**: Don't log DEBUG in production
2. **Avoid expensive operations**: Don't compute strings if log level is disabled
3. **Flush settings**: Logger auto-flushes on WARN and above
4. **Async logging** (if needed): Can be enabled for even better performance

## Example Output

### Console (with colors):
```
[2025-10-18 14:23:45.123] [INFO] === AI Platform Starting ===
[2025-10-18 14:23:45.234] [INFO] Using default config file: /path/to/config.txt
[2025-10-18 14:23:45.345] [INFO] Logger initialized successfully
[2025-10-18 14:23:45.456] [INFO] Model: Vehicle Detection
[2025-10-18 14:23:45.567] [DEBUG] Sending RC override - Pitch: 1527, Center in box: false
[2025-10-18 14:23:46.678] [WARN] Tracking confidence low: 0.65
[2025-10-18 14:23:47.789] [ERROR] MAVLink connection lost
```

### Log File:
```
[2025-10-18 14:23:45.123] [info] [main.cpp:15] === AI Platform Starting ===
[2025-10-18 14:23:45.234] [info] [main.cpp:18] Using default config file: /path/to/config.txt
[2025-10-18 14:23:45.345] [info] [logger.cpp:42] Logger initialized successfully
```

## Troubleshooting

### Logger not initialized
If you see no output, ensure `Logger::initialize()` is called in main.cpp before any LOG_* calls.

### Permission errors
If log file can't be created, check permissions on the `logs/` directory.

### Too much output
Increase log level:
```cpp
Logger::setLevel(spdlog::level::warn);  // Only warnings and errors
```

### Missing logs
Flush manually if needed:
```cpp
Logger::getLogger()->flush();
```
