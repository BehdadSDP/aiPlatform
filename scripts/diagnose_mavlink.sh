#!/bin/bash

echo "========================================="
echo "MAVLink Diagnostics for Raspberry Pi 5"
echo "========================================="

echo
echo "1. System Information:"
echo "----------------------"
cat /proc/version | head -1
uname -a

echo
echo "2. Available Serial Devices:"
echo "----------------------------"
ls -la /dev/serial* 2>/dev/null || echo "No /dev/serial* devices found"
ls -la /dev/ttyAMA* 2>/dev/null || echo "No /dev/ttyAMA* devices found"
ls -la /dev/ttyS* 2>/dev/null || echo "No /dev/ttyS* devices found"

echo
echo "3. UART Configuration:"
echo "----------------------"
echo "Boot config UART settings:"
grep -i uart /boot/config.txt 2>/dev/null || echo "Could not read /boot/config.txt"
echo
echo "Boot firmware config UART settings:"
grep -i uart /boot/firmware/config.txt 2>/dev/null || echo "Could not read /boot/firmware/config.txt"

echo
echo "4. System Logs (dmesg UART/TTY):"
echo "--------------------------------"
dmesg | grep -i -E "(uart|tty)" | tail -10

echo
echo "5. Current User Groups:"
echo "----------------------"
groups $USER

echo
echo "6. Serial Device Permissions:"
echo "-----------------------------"
for device in /dev/serial0 /dev/ttyAMA0 /dev/ttyS0; do
    if [ -e "$device" ]; then
        echo "$device: $(ls -la $device)"
    else
        echo "$device: Does not exist"
    fi
done

echo
echo "7. Process Using Serial Devices:"
echo "--------------------------------"
lsof /dev/serial* /dev/ttyAMA* /dev/ttyS* 2>/dev/null || echo "No processes found using serial devices"

echo
echo "8. Bluetooth Status (affects UART):"
echo "-----------------------------------"
systemctl is-active bluetooth 2>/dev/null || echo "Bluetooth service status unknown"
if command -v bluetoothctl >/dev/null 2>&1; then
    echo "show" | bluetoothctl | grep "Powered:" 2>/dev/null || echo "Could not check Bluetooth power status"
fi

echo
echo "9. Test Serial Write Permission:"
echo "-------------------------------"
for device in /dev/serial0 /dev/ttyAMA0 /dev/ttyS0; do
    if [ -e "$device" ]; then
        if [ -w "$device" ]; then
            echo "$device: Write permission OK"
        else
            echo "$device: NO write permission"
        fi
    fi
done

echo
echo "10. Raspberry Pi Model:"
echo "----------------------"
cat /proc/device-tree/model 2>/dev/null || echo "Could not determine Pi model"

echo
echo "========================================="
echo "Diagnostics complete!"
echo
echo "Common fixes for Raspberry Pi 5:"
echo "1. Enable UART: sudo raspi-config -> Interface Options -> Serial"
echo "2. Add user to dialout group: sudo usermod -a -G dialout \$USER"
echo "3. Check device exists and try: /dev/serial0 (preferred for Pi 5)"
echo "4. Reboot after changes: sudo reboot"
echo "5. For Pi 5, disable Bluetooth if needed: echo 'dtoverlay=disable-bt' | sudo tee -a /boot/firmware/config.txt"
echo "========================================="
