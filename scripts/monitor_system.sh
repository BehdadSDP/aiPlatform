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
    echo "Detailed Memory (MB):"
    # Get detailed memory info from /proc/meminfo
    total=$(grep "MemTotal:" /proc/meminfo | awk '{print $2/1024}')
    free=$(grep "MemFree:" /proc/meminfo | awk '{print $2/1024}')
    buffers=$(grep "Buffers:" /proc/meminfo | awk '{print $2/1024}')
    cached=$(grep "Cached:" /proc/meminfo | awk '{print $2/1024}')
    used=$(echo "$total - $free - $buffers - $cached" | bc -l)
    available=$(echo "$free + $buffers + $cached" | bc -l)
    
    printf "  Total: %.2f MB, Used: %.2f MB, Free: %.2f MB\n" $total $used $free
    printf "  Buffers: %.2f MB, Cached: %.2f MB, Available: %.2f MB\n" $buffers $cached $available
    
    echo "Temperature: $(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null | awk '{print $1/1000 "°C"}' || echo "N/A")"
    echo "Process Memory:"
    ps aux | grep -E "(ai|opencv)" | grep -v grep | awk '{print $2, $6/1024 "MB", $11}' | head -5
    echo "Open Files: $(lsof | wc -l)"
    echo "================================"
    sleep 30
done 