#!/bin/bash

# System monitoring script for AI Platform
# Run this script in a separate terminal to monitor system resources

echo "AI Platform System Monitor"
echo "Press Ctrl+C to stop monitoring"
echo "================================"

while true; do
    echo "$(date '+%Y-%m-%d %H:%M:%S')"
    echo "CPU Usage: $(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)%"
    echo "Memory Usage:"
    free -h | grep -E "Mem|Swap"
    echo "Temperature: $(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null | awk '{print $1/1000 "°C"}' || echo "N/A")"
    echo "Process Memory:"
    ps aux | grep -E "(ai|opencv)" | grep -v grep | awk '{print $2, $6/1024 "MB", $11}' | head -5
    echo "Open Files: $(lsof | wc -l)"
    echo "================================"
    sleep 30
done 