# Hazard Zone Visualization Control

## Overview

You can now control whether hazard zones are displayed on screen while keeping the detection functionality active. This allows for cleaner visual output while maintaining safety monitoring.

## Configuration Options

### New Visualization Settings

Add these settings to your `config.txt` under the `[hazard_zones]` section:

```ini
[hazard_zones]
# ... other settings ...

# Visualization settings
show_zones=1          # 0=hide zones on screen, 1=show zones on screen
show_zone_status=1    # 0=hide zone status info, 1=show zone status info
```

## Available Modes

### 1. **Full Visualization (Default)**
```ini
show_zones=1
show_zone_status=1
```
- Shows hazard zone rectangles
- Shows zone names and status
- Shows zone status panel in top-left corner
- Shows active/inactive indicators

### 2. **Hidden Zones with Status**
```ini
show_zones=0
show_zone_status=1
```
- Hides zone rectangles and names
- Still shows zone status panel
- Detection continues working
- Alarms still trigger

### 3. **Minimal Display**
```ini
show_zones=0
show_zone_status=0
```
- Hides all zone visualization
- Clean video output
- Detection and alarms still work
- Only violation alerts are shown

### 4. **Zones Only (No Status Panel)**
```ini
show_zones=1
show_zone_status=0
```
- Shows zone rectangles and names
- Hides the status panel
- Cleaner display with visible zones

## Important Notes

### ✅ **What Still Works When Hidden**
- Hazard zone violation detection
- Alarm triggering (visual/audio)
- Violation alerts on screen
- Console logging of violations
- All safety functionality

### 🎯 **What Gets Hidden**
- Zone boundary rectangles
- Zone name labels
- Active/inactive indicators
- Zone status information panel

## Use Cases

### **1. Production/Demo Mode**
```ini
# Clean display for presentations
show_zones=0
show_zone_status=0
```

### **2. Monitoring Mode**
```ini
# See status but not visual clutter
show_zones=0
show_zone_status=1
```

### **3. Setup/Debug Mode**
```ini
# Full visibility for configuration
show_zones=1
show_zone_status=1
```

### **4. Simplified View**
```ini
# Basic zones without extra info
show_zones=1
show_zone_status=0
```

## Configuration Examples

### Example 1: Hidden Zones Configuration
```bash
# Copy the hidden zones template
cp config/config_no_zones_display.txt config/config.txt
```

### Example 2: Manual Configuration
Edit your `config/config.txt`:
```ini
[hazard_zones]
alarm_enabled=1
visual_alarm=1
audio_alarm=0
alarm_cooldown_ms=2000

# Hide zones but keep status info
show_zones=0          # Hide zone rectangles
show_zone_status=1    # Keep status panel

# Your zones (still active for detection)
zone1=300,200,200,200
zone1_name=Safety Zone
zone1_active=1
```

## Runtime Behavior

### **Console Output**
The system will log the visualization settings on startup:
```
=== HAZARD ZONE CONFIGURATION ===
Alarm enabled: YES
Visual alarm: YES
Audio alarm: NO
Cooldown: 2000ms
Show zones: NO          ← New setting
Show status: YES        ← New setting
```

### **Detection Continues**
Even with `show_zones=0`, you'll still see:
- Console messages: "🚨 TRACKING MODE - Hazard zone violation detected!"
- Visual alarms when violations occur
- Alert overlays on detected objects

### **Performance Impact**
- **Hidden zones**: Slightly better performance (no drawing overhead)
- **Visible zones**: Normal performance
- **Detection**: No performance difference

## Migration Guide

### **Updating Existing Configurations**

**Old configuration:**
```ini
[hazard_zones]
alarm_enabled=1
visual_alarm=1
```

**New configuration:**
```ini
[hazard_zones]
alarm_enabled=1
visual_alarm=1

# Add visualization control
show_zones=1          # Keep existing behavior
show_zone_status=1    # Keep existing behavior
```

### **Default Behavior**
If you don't add the new settings, the system defaults to:
- `show_zones=1` (zones visible)
- `show_zone_status=1` (status visible)

This maintains backward compatibility with existing configurations.

## Testing

### **Verify Hidden Zones Still Work**
1. Set `show_zones=0` in config
2. Run the system
3. Walk into a configured hazard zone
4. Confirm:
   - No zone rectangles visible
   - Violation alerts still appear
   - Console shows violation messages

### **Quick Toggle Test**
```bash
# Test with zones visible
echo "show_zones=1" >> config/config.txt
./ai

# Test with zones hidden
sed -i 's/show_zones=1/show_zones=0/' config/config.txt
./ai
```

This gives you complete control over the visual appearance while maintaining full safety functionality! 