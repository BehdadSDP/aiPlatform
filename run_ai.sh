#!/bin/bash
# AI Platform Launch Script for Raspberry Pi 5
# This script sets up the environment and launches the application

# Set proper permissions for runtime directory (suppress warning)
export XDG_RUNTIME_DIR=/run/user/$(id -u)

# Force Qt to use xcb platform plugin (X11)
export QT_QPA_PLATFORM=xcb

# Disable GTK accessibility features that can conflict with Qt
export NO_AT_BRIDGE=1

# Prevent Qt from loading GTK platform theme
export QT_QPA_PLATFORMTHEME=""

# Set Qt scaling for better display on Pi
export QT_AUTO_SCREEN_SCALE_FACTOR=0
export QT_SCALE_FACTOR=1

# Change to application directory
cd "$(dirname "$0")"

# Parse command line arguments
GUI_MODE=true
CONFIG_PATH="/home/pi5/shared_folder/aiPlatform/config/config.txt"

for arg in "$@"; do
    case $arg in
        --no-gui|-ng)
            GUI_MODE=false
            shift
            ;;
        --config=*)
            CONFIG_PATH="${arg#*=}"
            shift
            ;;
        -c)
            CONFIG_PATH="$2"
            shift 2
            ;;
        *)
            # Unknown option
            ;;
    esac
done

# Launch application
if [ "$GUI_MODE" = true ]; then
    echo "Starting AI Platform with GUI..."
    ./ai "$@"
else
    echo "Starting AI Platform in console mode..."
    ./ai --no-gui "$@"
fi
