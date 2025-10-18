# Smooth Navigation Control Implementation

## Overview
This document describes the smooth navigation control system that prevents harsh and aggressive drone responses to tracking commands.

## Problem
The drone was responding too quickly and aggressively to RC override commands, causing:
- Sudden jerky movements
- Potential instability
- Uncomfortable flight characteristics
- Risk of overshooting the target

## Solution
Implemented a **dual-layer smoothing system**:

### 1. Output Slew Rate Limiting
**Purpose**: Prevents sudden jumps in RC output values

**How it works**:
- Limits the maximum rate of change in PWM values per second
- Gradually transitions from current output to desired output
- Example: With 50 PWM/sec limit, going from 1500→1600 takes 2 seconds instead of instant

**Implementation**:
```cpp
int applySlewRateLimit(int desiredOutput, int previousOutput, float deltaTime)
```

**Configuration** (in `config.txt`):
```ini
[navigation]
max_rc_change_rate=50  # PWM units per second
```

**Tuning Guide**:
- **20-30**: Very smooth, slow response (gentle tracking)
- **40-60**: Balanced smoothness and responsiveness (recommended)
- **70-100**: Faster response, less smooth (aggressive tracking)

### 2. Reduced Maximum Deviation
**Before**: ±200 PWM from neutral (1300-1700 range)
**After**: ±100 PWM from neutral (1400-1600 range)

**Effect**: Limits maximum tilt angle, resulting in:
- Gentler movements
- More predictable behavior
- Safer operation

### 3. Existing Error Signal Filtering
**Already implemented**: Low-pass filter (Exponential Moving Average)
- Smooths the input error signal before PID processing
- Configurable via `filter_alpha` parameter

**Configuration**:
```ini
[navigation]
filter_alpha=0.3  # 0.0-1.0
```

**Tuning Guide**:
- **0.1-0.2**: Heavy filtering, very smooth but slower response
- **0.3-0.4**: Balanced (recommended)
- **0.5-0.7**: Light filtering, faster but less smooth

## Complete Signal Flow

```
Raw Error → Low-Pass Filter → PID Controller → RC Commands → Slew Rate Limiter → Output
   (tracking)     (filter_alpha)    (Kp,Ki,Kd)   (±100 max)   (max_rc_change_rate)  (to drone)
```

## Configuration Parameters

### In `config/config.txt`:

```ini
[navigation]
# RC output slew rate limiter (smoothness control)
# Maximum RC change per second in PWM units (default: 50)
# Lower values = smoother/slower response (20-30 for very smooth)
# Higher values = faster response (80-100 for more responsive)
max_rc_change_rate=50

# Error signal filter smoothing factor (0.0-1.0)
# Lower values = more smoothing (0.1-0.2 for smooth)
# Higher values = less smoothing/faster response (0.4-0.5)
filter_alpha=0.3
```

## New Methods Added

### NavigationUnit Class

**Public Methods**:
```cpp
void setMaxRCChangeRate(float maxChangeRate);
```
- Sets the slew rate limit for RC outputs
- Range: 5-500 PWM/sec (clamped)

**Private Methods**:
```cpp
int applySlewRateLimit(int desiredOutput, int previousOutput, float deltaTime);
```
- Applies rate limiting to prevent sudden output changes
- Returns the limited output value

**New Member Variables**:
```cpp
float m_maxRCChangeRate = 50.0f;      // Slew rate limit
int m_previousRollOutput = 1500;       // Previous roll output
int m_previousPitchOutput = 1500;      // Previous pitch output
bool m_firstOutputUpdate = true;       // First update flag
```

## Files Modified

1. **include/navigation_unit.h**
   - Added slew rate member variables
   - Added `setMaxRCChangeRate()` method
   - Added `applySlewRateLimit()` private method

2. **src/navigation_unit.cpp**
   - Implemented `setMaxRCChangeRate()` method
   - Implemented `applySlewRateLimit()` method
   - Modified both `generateControlCommands()` overloads to use slew rate limiting
   - Reduced `max_deviation` from 200 to 100

3. **config/config.txt**
   - Added `[navigation]` section
   - Added `max_rc_change_rate` parameter
   - Added `filter_alpha` parameter (documentation)

4. **src/application.cpp**
   - Added configuration loading for navigation parameters
   - Applied settings during initialization
   - Added console output showing navigation configuration

## Testing Recommendations

### Phase 1: Very Smooth (Safe Testing)
```ini
max_rc_change_rate=30
filter_alpha=0.2
```
Expected: Very gentle, slow movements. Good for initial testing.

### Phase 2: Balanced (Recommended)
```ini
max_rc_change_rate=50
filter_alpha=0.3
```
Expected: Smooth but responsive. Good for normal operations.

### Phase 3: Responsive (If needed)
```ini
max_rc_change_rate=80
filter_alpha=0.4
```
Expected: Faster response with moderate smoothness.

## Benefits

1. **Stability**: Gradual changes prevent sudden oscillations
2. **Safety**: Reduced maximum tilt angles
3. **Predictability**: Consistent response characteristics
4. **Comfort**: Smoother flight for observers/pilots
5. **Battery**: Less aggressive maneuvers = better efficiency
6. **Tunable**: Easy to adjust via config file without recompilation

## Monitoring

The navigation control outputs are logged and visualized:
- Current error values (filtered)
- Roll/Pitch RC outputs
- Whether commands are being sent

Use these to verify smooth transitions and appropriate response times.

## Notes

- The slew rate limiter is applied **after** PID control and clamping
- Both roll and pitch channels are independently limited
- First command after initialization is not rate-limited (prevents delay at startup)
- The system maintains smooth transitions even when target suddenly moves
- Rate limiting works in conjunction with existing low-pass filter for optimal smoothness
