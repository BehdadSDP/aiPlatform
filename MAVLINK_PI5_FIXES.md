# MAVLink Raspberry Pi 5 Fixes

## Issues Identified and Fixed

### 1. Serial Device Path Issues
**Problem**: Raspberry Pi 5 uses different serial device paths than previous models.

**Fix**: Updated code to try multiple device paths in order of preference:
- `/dev/serial0` (Raspberry Pi 5 primary UART - **preferred**)
- `/dev/ttyAMA0` (Traditional Pi UART)
- `/dev/ttyS0` (Mini UART fallback)

### 2. Example Code Bug
**Problem**: `mavlink_example.cpp` called `initialize()` without parameters, causing undefined behavior.

**Fix**: Updated example to:
- Try callback mode first (for testing without hardware)
- Fall back to serial devices with proper error handling
- Provide clear error messages with troubleshooting steps

### 3. Permission and Error Handling
**Problem**: Poor error messages when serial devices are inaccessible.

**Fix**: Added comprehensive error checking:
- Check if device exists before opening
- Check read/write permissions
- Provide specific troubleshooting advice
- Better error messages with errno details

### 4. Debug Output
**Problem**: No visibility into what data is being sent.

**Fix**: Added debug output to show:
- Bytes being sent via serial (hex format)
- Successful transmission confirmation
- Detailed error reporting

## Configuration Changes

### Updated `config/config.txt`
```ini
# Serial port for MAVLink communication (Raspberry Pi 5 primary UART)
serial_port=/dev/serial0
```

### New Diagnostic Script
Created `scripts/diagnose_mavlink.sh` to help identify configuration issues:
- Check available serial devices
- Verify UART configuration
- Check user permissions
- Test device accessibility

## Testing Steps

1. **Run diagnostics**:
   ```bash
   ./scripts/diagnose_mavlink.sh
   ```

2. **Build and test**:
   ```bash
   qmake mavlink_example.pro
   make
   ./mavlink_example
   ```

3. **Expected output**:
   - Should try callback mode first (always works)
   - Shows heartbeat messages in hex format
   - If serial hardware available, will try real devices

## Common Raspberry Pi 5 Setup Issues

### 1. UART Not Enabled
```bash
sudo raspi-config
# Interface Options -> Serial Port
# Shell access: No
# Hardware enabled: Yes
```

### 2. User Permissions
```bash
sudo usermod -a -G dialout $USER
# Log out and back in, or reboot
```

### 3. Device Permissions
```bash
# Check permissions
ls -la /dev/serial*
# Fix if needed
sudo chmod 666 /dev/serial0
```

### 4. Bluetooth Conflicts (Pi 5)
For Pi 5, if using `/dev/serial0`, you might need to disable Bluetooth:
```bash
echo 'dtoverlay=disable-bt' | sudo tee -a /boot/firmware/config.txt
sudo reboot
```

### 5. Boot Configuration
Ensure UART is enabled in boot config:
```bash
# For Pi 5, check both locations:
sudo nano /boot/firmware/config.txt
# OR
sudo nano /boot/config.txt

# Add these lines:
enable_uart=1
dtoverlay=uart0
```

## Verification

### Working Output Example
```
MAVLink Handler Example
======================
Trying callback mode (no hardware required)...
Initialized in callback mode successfully
MAVLink Handler initialized using: callback mode
Heartbeat started with interval: 1000ms

Sending heartbeat messages...
Press Ctrl+C to stop
========================
MAVLink Message Output (9 bytes): FD 09 00 00 00 01 01 00 00 00 04 00 00 00 03 02 03 A8 95
Sent MAVLink message ID: 0, Length: 9 bytes
```

### Hardware Output Example (when serial works)
```
Trying callback mode (no hardware required)...
Failed to initialize callback mode
Trying serial device: /dev/serial0
Serial device /dev/serial0 initialized successfully
MAVLink Handler initialized using: /dev/serial0
Sending 9 bytes via serial: FD 09 00 00 00 01 01 00 00 ...
Successfully sent 9 bytes via serial
```

## Key Improvements

1. **Robust device detection**: Tries multiple serial devices automatically
2. **Better error messages**: Clear guidance on what to fix
3. **Fallback mode**: Callback mode works without hardware for testing
4. **Debug output**: Visibility into data transmission
5. **Pi 5 compatibility**: Correct device paths and configuration
6. **Diagnostic tools**: Script to identify configuration issues

## Next Steps

If issues persist after these fixes:
1. Run the diagnostic script: `./scripts/diagnose_mavlink.sh`
2. Check the output for specific configuration problems
3. Verify UART is enabled and user has proper permissions
4. Test with loopback (connect TX to RX) to verify hardware functionality
5. Use a USB-to-serial adapter as alternative if built-in UART fails
