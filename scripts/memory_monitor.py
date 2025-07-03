#!/usr/bin/env python3
"""
Memory Monitoring Script for AI Platform
Monitors memory usage and tracks improvements from optimizations
"""

import psutil
import time
import csv
import os
from datetime import datetime
import matplotlib.pyplot as plt
import numpy as np

class MemoryMonitor:
    def __init__(self, log_file="memory_usage.csv"):
        self.log_file = log_file
        self.start_time = time.time()
        self.memory_data = []
        
    def get_memory_info(self):
        """Get detailed memory information"""
        memory = psutil.virtual_memory()
        swap = psutil.swap_memory()
        
        return {
            'timestamp': datetime.now().isoformat(),
            'total_mb': memory.total / (1024 * 1024),
            'used_mb': memory.used / (1024 * 1024),
            'available_mb': memory.available / (1024 * 1024),
            'percent': memory.percent,
            'swap_total_mb': swap.total / (1024 * 1024),
            'swap_used_mb': swap.used / (1024 * 1024),
            'swap_percent': swap.percent
        }
    
    def log_memory(self):
        """Log current memory usage"""
        memory_info = self.get_memory_info()
        self.memory_data.append(memory_info)
        
        # Write to CSV
        file_exists = os.path.exists(self.log_file)
        with open(self.log_file, 'a', newline='') as csvfile:
            fieldnames = memory_info.keys()
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            
            if not file_exists:
                writer.writeheader()
            
            writer.writerow(memory_info)
        
        # Print current status
        print(f"[{memory_info['timestamp']}] "
              f"Memory: {memory_info['used_mb']:.1f}/{memory_info['total_mb']:.1f}MB "
              f"({memory_info['percent']:.1f}%) "
              f"Swap: {memory_info['swap_used_mb']:.1f}/{memory_info['swap_total_mb']:.1f}MB "
              f"({memory_info['swap_percent']:.1f}%)")
    
    def monitor_continuously(self, interval=5):
        """Monitor memory continuously"""
        print(f"Starting memory monitoring. Logging to: {self.log_file}")
        print("Press Ctrl+C to stop monitoring")
        print("-" * 80)
        
        try:
            while True:
                self.log_memory()
                time.sleep(interval)
        except KeyboardInterrupt:
            print("\nMonitoring stopped.")
            self.generate_report()
    
    def generate_report(self):
        """Generate a memory usage report"""
        if not self.memory_data:
            print("No memory data collected.")
            return
        
        print("\n" + "="*50)
        print("MEMORY USAGE REPORT")
        print("="*50)
        
        # Calculate statistics
        used_mb = [d['used_mb'] for d in self.memory_data]
        percent = [d['percent'] for d in self.memory_data]
        
        print(f"Monitoring duration: {time.time() - self.start_time:.1f} seconds")
        print(f"Data points collected: {len(self.memory_data)}")
        print(f"Average memory usage: {np.mean(used_mb):.1f}MB ({np.mean(percent):.1f}%)")
        print(f"Peak memory usage: {np.max(used_mb):.1f}MB ({np.max(percent):.1f}%)")
        print(f"Minimum memory usage: {np.min(used_mb):.1f}MB ({np.min(percent):.1f}%)")
        print(f"Memory usage std dev: {np.std(used_mb):.1f}MB")
        
        # Generate plot
        self.plot_memory_usage()
    
    def plot_memory_usage(self):
        """Generate a plot of memory usage over time"""
        if not self.memory_data:
            return
        
        timestamps = [datetime.fromisoformat(d['timestamp']) for d in self.memory_data]
        used_mb = [d['used_mb'] for d in self.memory_data]
        percent = [d['percent'] for d in self.memory_data]
        
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
        
        # Plot memory usage in MB
        ax1.plot(timestamps, used_mb, 'b-', linewidth=2)
        ax1.set_ylabel('Memory Usage (MB)')
        ax1.set_title('AI Platform Memory Usage Over Time')
        ax1.grid(True, alpha=0.3)
        
        # Plot memory usage percentage
        ax2.plot(timestamps, percent, 'r-', linewidth=2)
        ax2.set_ylabel('Memory Usage (%)')
        ax2.set_xlabel('Time')
        ax2.grid(True, alpha=0.3)
        
        plt.tight_layout()
        
        # Save plot
        plot_file = "memory_usage_plot.png"
        plt.savefig(plot_file, dpi=300, bbox_inches='tight')
        print(f"Memory usage plot saved to: {plot_file}")
        
        # Show plot if possible
        try:
            plt.show()
        except:
            pass

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Monitor AI Platform memory usage')
    parser.add_argument('--interval', type=int, default=5, 
                       help='Monitoring interval in seconds (default: 5)')
    parser.add_argument('--log-file', type=str, default='memory_usage.csv',
                       help='Log file path (default: memory_usage.csv)')
    
    args = parser.parse_args()
    
    monitor = MemoryMonitor(args.log_file)
    monitor.monitor_continuously(args.interval)

if __name__ == "__main__":
    main() 