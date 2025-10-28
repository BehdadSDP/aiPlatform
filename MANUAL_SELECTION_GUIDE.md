# Manual Object Selection Feature

## Overview
The manual object selection feature allows users to manually choose which detected object to track when multiple objects are detected in the same frame. This is particularly useful when the automatic selection strategies don't choose the desired target.

## Configuration

To enable manual selection, set the `selection_strategy` parameter in your `config/config.txt` file:

```ini
# Selection strategy: 0=highest confidence, 1=upper box, 2=lower box, 3=rightmost, 4=leftmost, 5=similarity, 6=manual selection
selection_strategy=6
```

## How It Works

1. **Detection Phase**: When the object detection model detects objects, the system will:
   - Display numbered bounding boxes around each detected object on the video window
   - Show confidence scores and bounding box coordinates for each object in the console
   - Continue running detection and showing numbered boxes without pausing
   - Require manual confirmation even for single objects
   - Allow you to enter your selection at any time when ready

2. **User Selection**: The system will prompt you in the console with:
   ```
   === MANUAL OBJECT SELECTION ===
   Object detected. Please confirm to track it:        (for single object)
   OR
   Multiple objects detected. Please select which object to track:  (for multiple objects)
   Look at the video window to see the numbered bounding boxes.
   Enter object number (1-N) when ready:
   ```

3. **Visual Feedback**: The video display will show:
   - Large numbered circles in the center of each bounding box
   - Thicker bounding box borders for better visibility
   - Instructions at the bottom of the screen
   - Numbered labels showing confidence percentages

4. **Selection**: Enter the number corresponding to the object you want to track, and the system will:
   - Select that object for tracking immediately
   - Continue with normal tracking operation
   - The numbered boxes will disappear and normal tracking visualization will resume

## Visual Indicators

When manual selection mode is active, you'll see:
- **Numbered bounding boxes**: Each detection has a large number in the center
- **Thicker borders**: Bounding boxes are drawn with 3-pixel thickness
- **Instruction text**: "MANUAL SELECTION MODE - Check console for selection prompt"
- **Numbered labels**: Each detection shows "[N] ClassName: Confidence%"

## Usage Tips

1. **Non-blocking Selection**: The detection continues running, so you can observe the numbered objects and make your selection when ready
2. **Background Input**: You can enter your selection at any time - the system reads it in the background
3. **Fallback**: If you enter an invalid number, the system will automatically select the highest confidence object
4. **Single Object Confirmation**: Even with only one object detected, you must enter "1" to confirm tracking
5. **Low Confidence**: Only objects with confidence > 15% are shown for selection

## Example Workflow

1. Start the application with `selection_strategy=6`
2. When objects are detected, you'll see numbered boxes continuously
3. Check the console for the selection prompt
4. Observe the numbered objects on screen to decide which one to track
5. Enter the number of the object you want to track when ready (even for single objects, enter "1")
6. The system will immediately start tracking your selected object
7. Normal tracking continues with regular visualization

This feature is particularly useful for:
- Tracking specific vehicles in traffic
- Selecting the correct person in a crowd
- Choosing the right object when multiple similar objects are present
- Fine-tuning object selection for specific scenarios
