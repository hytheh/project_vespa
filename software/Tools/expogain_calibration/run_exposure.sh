#!/bin/bash
# VESPA Exposure Tool - Auto-Environment Wrapper

# Get the absolute directory where this script is located
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

echo "[LAUNCHER] Starting VESPA Exposure & Gain Calibrator..."

# Execute the Python script using the VENV's specific interpreter with sudo
sudo $DIR/venv/bin/python3 $DIR/app_EG.py
