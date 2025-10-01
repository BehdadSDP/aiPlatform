# MAVLink Handler for Raspberry Pi

This MAVLink handler provides functionality to send heartbeat messages over serial UART communication, designed specifically for Raspberry Pi integration with drone/UAV systems.

## Features

- **Serial UART Communication**: Direct communication via Raspberry Pi UART pins
- **MAVLink Protocol**: Uses the common dialect with full message support
- **Heartbeat Messages**: Automatic periodic heartbeat transmission
- **Configurable Parameters**: Customizable vehicle type, autopilot, and system status
- **Error Handling**: Comprehensive error detection and reporting
- **Debug Mode**: Optional callback mode for testing without hardware

## Hardware Setup

### Raspberry Pi UART Configuration

The Raspberry Pi has two UART interfaces:
- **Primary UART (ttyAMA0)**: Hardware UART (recommended for MAVLink)
- **Secondary UART (ttyS0)**: Mini UART (less reliable due to frequency scaling)

### Enable UART on Raspberry Pi

1. **Configure via raspi-config**:
   ```bash
   sudo raspi-config
   ```
   - Navigate to: `Interface Options` → `Serial Port`
   - "Would you like a login shell to be accessible over serial?" → **No** (for MAVLink only)
   - "Would you like the serial port hardware to be enabled?" → **Yes**

2. **Edit boot configuration**:
   ```bash
   sudo nano /boot/config.txt
   ```
   Add these lines:
   ```
   enable_uart=1
   dtoverlay=uart0
   ```

3. **Disable Bluetooth (optional, for better UART performance)**:
   ```bash
   sudo nano /boot/config.txt
   ```
   Add:
   ```
   dtoverlay=disable-bt
   ```

4. **Reboot the system**:
   ```bash
   sudo reboot
   ```

5. **Verify UART is available**:
   ```bash
   ls -la /dev/tty*
   ```
   You should see `/dev/ttyAMA0` or `/dev/ttyS0`

### Wiring

Connect your MAVLink-compatible device to the Raspberry Pi UART pins:

```
Raspberry Pi GPIO    →    MAVLink Device
Pin 8 (GPIO 14, TX)  →    RX
Pin 10 (GPIO 15, RX) →    TX
Pin 6 (GND)          →    GND
```

**Warning**: Raspberry Pi GPIO pins operate at 3.3V. Ensure your MAVLink device is compatible or use a level shifter for 5V devices.

## Software Usage

### Compilation

1. **Using the standalone example**:
   ```bash
   cd /path/to/aiPlatform
   qmake mavlink_example.pro
   make
   ```

2. **Adding to existing project**:
   Add to your `.pro` file:
   ```qmake
   SOURCES += src/mavlink_handler.cpp
   HEADERS += include/mavlink_handler.h
   INCLUDEPATH += include/Output
   ```

### Running the Example

1. **With serial communication (default)**:
   ```bash
   ./mavlink_example                           # Use /dev/ttyAMA0 at 57600 baud
   ./mavlink_example /dev/ttyS0                # Use /dev/ttyS0 at 57600 baud
   ./mavlink_example /dev/ttyAMA0 115200       # Use /dev/ttyAMA0 at 115200 baud
   ```

2. **With debug callback mode**:
   ```bash
   ./mavlink_example --callback
   ```

3. **Fix permissions if needed**:
   ```bash
   sudo chmod 666 /dev/ttyAMA0
   ```

### Code Usage

```cpp
#include "mavlink_handler.h"

// Create handler
MavlinkHandler mavlink(1, 1);  // System ID: 1, Component ID: 1

// Initialize with serial communication
if (mavlink.initializeUART("/dev/ttyAMA0", 57600) && mavlink.start()) {
    // MAVLink is now ready for communication
    
    // Your application loop here
    while (running) {
        sleep(1);
    }
    
    // Cleanup
    mavlink.stop();
}
```

## Troubleshooting

### Common Issues

1. **Permission Denied**:
   ```bash
   sudo chmod 666 /dev/ttyAMA0
   # Or run with sudo (not recommended for production)
   ```

2. **Device Not Found**:
   - Check if UART is enabled: `ls -la /dev/tty*`
   - Verify boot configuration
   - Try different device: `/dev/ttyS0` instead of `/dev/ttyAMA0`

3. **No Data Transmission**:
   - Check wiring connections
   - Verify baud rate matches connected device
   - Test with loopback (connect TX to RX)

4. **Bluetooth Conflicts**:
   - Disable Bluetooth if using `/dev/ttyAMA0`
   - Add `dtoverlay=disable-bt` to `/boot/config.txt`

### Testing with Ground Control Software

1. **MAVProxy** (command line):
   ```bash
   pip install MAVProxy
   mavproxy.py --master=/dev/ttyUSB0 --baudrate=57600
   ```

2. **QGroundControl**: Connect via serial port configuration

3. **Mission Planner**: Configure connection settings for serial port

### Verification Commands

```bash
# Check UART status
dmesg | grep tty

# Monitor serial traffic
sudo cat /dev/ttyAMA0

# Test serial port with screen
screen /dev/ttyAMA0 57600
```

## Supported Baud Rates

- 9600
- 19200
- 38400
- 57600 (recommended)
- 115200
- 230400
- 460800
- 921600

## Message Format

The handler sends standard MAVLink v2.0 heartbeat messages containing:
- System ID and Component ID
- Vehicle type (e.g., quadrotor, fixed wing)
- Autopilot type (e.g., ArduPilot, PX4)
- Flight mode and status
- Custom mode flags

## Integration Notes

- This handler is designed to work with the existing AI platform architecture
- Can be easily extended to send additional MAVLink messages (GPS, IMU, battery status)
- Thread-safe implementation with proper cleanup
- Compatible with MAVLink v1.0 and v2.0 ground control stations