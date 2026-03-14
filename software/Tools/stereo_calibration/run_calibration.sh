#!/bin/bash
# VESPA Stereo Calibrator - Auto-Environment Wrapper
# This script ensures the app runs in its isolated container.

# Get the absolute directory where this script is located
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

echo "[LAUNCHER] Starting VESPA Stereo Calibrator..."

# Execute the python script using the VENV's specific interpreter
$DIR/venv/bin/python3 $DIR/app_ST.py