#!/bin/bash

# MAVLink ARM Command Diagnostic Script
# This script helps diagnose MAVLink connection and arm command issues

echo "🔍 MAVLink ARM Command Diagnostic Tool"
echo "======================================"

# Check if UART device exists
UART_DEVICE="/dev/ttyAMA0"
echo "1. Checking UART device: $UART_DEVICE"
if [ -e "$UART_DEVICE" ]; then
    echo "   ✅ UART device exists"
    ls -la "$UART_DEVICE"
else
    echo "   ❌ UART device not found"
    echo "   Available devices:"
    ls -la /dev/tty* | grep -E '(ttyAMA|ttyS|serial)'
fi

# Check UART permissions
echo ""
echo "2. Checking UART permissions:"
if [ -r "$UART_DEVICE" ] && [ -w "$UART_DEVICE" ]; then
    echo "   ✅ UART device is readable and writable"
else
    echo "   ❌ UART device permission issues"
    echo "   Current permissions:"
    ls -la "$UART_DEVICE" 2>/dev/null || echo "   Device not accessible"
    echo "   Solutions:"
    echo "   - Add user to dialout group: sudo usermod -a -G dialout \$USER"
    echo "   - Set permissions: sudo chmod 666 $UART_DEVICE"
fi

# Check Raspberry Pi 5 UART configuration
echo ""
echo "3. Checking Raspberry Pi 5 UART configuration:"
echo "   Boot config.txt UART settings:"
if grep -q "enable_uart=1" /boot/config.txt 2>/dev/null; then
    echo "   ✅ enable_uart=1 found in /boot/config.txt"
else
    echo "   ❌ enable_uart=1 not found in /boot/config.txt"
fi

if grep -q "dtoverlay=uart0" /boot/config.txt 2>/dev/null; then
    echo "   ✅ dtoverlay=uart0 found in /boot/config.txt"
else
    echo "   ❌ dtoverlay=uart0 not found in /boot/config.txt"
fi

# Check for conflicting console settings
echo "   Console settings in cmdline.txt:"
if grep -q "console=ttyAMA0" /boot/cmdline.txt 2>/dev/null; then
    echo "   ⚠️  console=ttyAMA0 found - this may conflict with MAVLink"
    echo "   Remove 'console=ttyAMA0,115200' from /boot/cmdline.txt"
else
    echo "   ✅ No console conflict detected"
fi

# Check system groups
echo ""
echo "4. Checking user groups:"
if groups | grep -q dialout; then
    echo "   ✅ User is in dialout group"
else
    echo "   ❌ User not in dialout group"
    echo "   Run: sudo usermod -a -G dialout \$USER"
fi

# Check for running processes using UART
echo ""
echo "5. Checking for processes using UART:"
UART_PROCESSES=$(lsof "$UART_DEVICE" 2>/dev/null)
if [ -n "$UART_PROCESSES" ]; then
    echo "   ⚠️  Processes using UART device:"
    echo "$UART_PROCESSES"
else
    echo "   ✅ No processes using UART device"
fi

# Test UART communication
echo ""
echo "6. Testing UART communication:"
echo "   Attempting to send test data..."

# Try to open UART for testing
if timeout 2 bash -c "echo 'test' > $UART_DEVICE 2>/dev/null"; then
    echo "   ✅ UART write test successful"
else
    echo "   ❌ UART write test failed"
fi

echo ""
echo "📋 MAVLink ARM Command Troubleshooting Summary:"
echo "=============================================="
echo "If ARM commands are not working, check:"
echo ""
echo "1. UART Configuration:"
echo "   - Ensure enable_uart=1 in /boot/config.txt"
echo "   - Add dtoverlay=uart0 in /boot/config.txt"
echo "   - Remove console=ttyAMA0 from /boot/cmdline.txt"
echo "   - Reboot after changes: sudo reboot"
echo ""
echo "2. Permissions:"
echo "   - Add user to dialout: sudo usermod -a -G dialout \$USER"
echo "   - Log out and back in for group changes to take effect"
echo ""
echo "3. Flight Controller:"
echo "   - Ensure flight controller is connected and powered"
echo "   - Check flight controller is in correct mode (not failsafe)"
echo "   - Verify GPS lock and other pre-arm checks are satisfied"
echo "   - Try different force parameters (21196 for PX4, 2989 for ArduPilot)"
echo ""
echo "4. MAVLink Settings:"
echo "   - Verify correct baud rate (115200 is common)"
echo "   - Check system_id and component_id match flight controller"
echo "   - Ensure target_system_id is correct (usually 1)"
echo ""
echo "5. Testing:"
echo "   - Use QGroundControl or Mission Planner to test MAVLink connection"
echo "   - Check if manual ARM commands work from ground station"
echo "   - Monitor flight controller logs for command reception"
echo ""
echo "🔧 Quick Fix Commands:"
echo "sudo usermod -a -G dialout \$USER"
echo "sudo chmod 666 $UART_DEVICE"
echo "sudo reboot"
