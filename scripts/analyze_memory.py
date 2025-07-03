#!/usr/bin/env python3
"""
Memory Analysis Script for AI Platform
Analyzes the detailed memory usage logs from the enhanced resource monitor
"""

import pandas as pd
import matplotlib.pyplot as plt
import sys
import os
from datetime import datetime

def analyze_memory_logs(log_file_path):
    """Analyze memory usage from the enhanced resource monitor logs"""
    
    if not os.path.exists(log_file_path):
        print(f"Error: Log file not found: {log_file_path}")
        return
    
    try:
        # Read the CSV file
        df = pd.read_csv(log_file_path)
        
        # Convert timestamp to datetime
        df['Timestamp'] = pd.to_datetime(df['Timestamp'])
        
        print("=== AI Platform Memory Analysis ===")
        print(f"Log file: {log_file_path}")
        print(f"Time range: {df['Timestamp'].min()} to {df['Timestamp'].max()}")
        print(f"Total records: {len(df)}")
        print()
        
        # Memory usage statistics
        print("=== Memory Usage Statistics ===")
        print(f"Average Used Memory: {df['Used Memory (MB)'].mean():.2f} MB")
        print(f"Average Buffers: {df['Buffers (MB)'].mean():.2f} MB")
        print(f"Average Cache: {df['Cache (MB)'].mean():.2f} MB")
        print(f"Average Available: {df['Available (MB)'].mean():.2f} MB")
        print()
        
        print(f"Peak Used Memory: {df['Used Memory (MB)'].max():.2f} MB")
        print(f"Peak Buffers: {df['Buffers (MB)'].max():.2f} MB")
        print(f"Peak Cache: {df['Cache (MB)'].max():.2f} MB")
        print()
        
        # Memory utilization percentage
        df['Memory Utilization %'] = (df['Used Memory (MB)'] / df['Total Memory (MB)']) * 100
        print(f"Average Memory Utilization: {df['Memory Utilization %'].mean():.2f}%")
        print(f"Peak Memory Utilization: {df['Memory Utilization %'].max():.2f}%")
        print()
        
        # Memory component breakdown
        print("=== Memory Component Breakdown ===")
        avg_used = df['Used Memory (MB)'].mean()
        avg_buffers = df['Buffers (MB)'].mean()
        avg_cache = df['Cache (MB)'].mean()
        avg_available = df['Available (MB)'].mean()
        total_avg = avg_used + avg_buffers + avg_cache + avg_available
        
        print(f"Used Memory: {avg_used:.2f} MB ({avg_used/total_avg*100:.1f}%)")
        print(f"Buffers: {avg_buffers:.2f} MB ({avg_buffers/total_avg*100:.1f}%)")
        print(f"Cache: {avg_cache:.2f} MB ({avg_cache/total_avg*100:.1f}%)")
        print(f"Available: {avg_available:.2f} MB ({avg_available/total_avg*100:.1f}%)")
        print()
        
        # Create visualization
        create_memory_plots(df)
        
    except Exception as e:
        print(f"Error analyzing logs: {e}")

def create_memory_plots(df):
    """Create memory usage visualization plots"""
    
    # Set up the plot
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(15, 10))
    fig.suptitle('AI Platform Memory Usage Analysis', fontsize=16)
    
    # Plot 1: Memory components over time
    ax1.plot(df['Timestamp'], df['Used Memory (MB)'], label='Used', linewidth=2)
    ax1.plot(df['Timestamp'], df['Buffers (MB)'], label='Buffers', linewidth=2)
    ax1.plot(df['Timestamp'], df['Cache (MB)'], label='Cache', linewidth=2)
    ax1.plot(df['Timestamp'], df['Available (MB)'], label='Available', linewidth=2)
    ax1.set_title('Memory Components Over Time')
    ax1.set_ylabel('Memory (MB)')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # Plot 2: Memory utilization percentage
    utilization = (df['Used Memory (MB)'] / df['Total Memory (MB)']) * 100
    ax2.plot(df['Timestamp'], utilization, color='red', linewidth=2)
    ax2.set_title('Memory Utilization Percentage')
    ax2.set_ylabel('Utilization (%)')
    ax2.grid(True, alpha=0.3)
    
    # Plot 3: Memory component distribution (pie chart)
    avg_used = df['Used Memory (MB)'].mean()
    avg_buffers = df['Buffers (MB)'].mean()
    avg_cache = df['Cache (MB)'].mean()
    avg_available = df['Available (MB)'].mean()
    
    labels = ['Used', 'Buffers', 'Cache', 'Available']
    sizes = [avg_used, avg_buffers, avg_cache, avg_available]
    colors = ['#ff9999', '#66b3ff', '#99ff99', '#ffcc99']
    
    ax3.pie(sizes, labels=labels, colors=colors, autopct='%1.1f%%', startangle=90)
    ax3.set_title('Average Memory Distribution')
    
    # Plot 4: CPU vs Memory correlation
    ax4.scatter(df['CPU Usage (%)'], df['Used Memory (MB)'], alpha=0.6)
    ax4.set_xlabel('CPU Usage (%)')
    ax4.set_ylabel('Used Memory (MB)')
    ax4.set_title('CPU vs Memory Usage Correlation')
    ax4.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    # Save the plot
    output_file = 'memory_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Memory analysis plot saved as: {output_file}")
    
    # Show the plot
    plt.show()

def main():
    """Main function"""
    if len(sys.argv) > 1:
        log_file = sys.argv[1]
    else:
        log_file = "logs/resource_usage.csv"
    
    analyze_memory_logs(log_file)

if __name__ == "__main__":
    main() 