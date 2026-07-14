#!/bin/bash
# VESPA Exposure Tool - Auto-Environment Wrapper

# Get the absolute directory where this script is located
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

echo "[LAUNCHER] Starting VESPA Exposure & Gain Calibrator..."

# Create the virtualenv and install dependencies on first run.
if [ ! -d "$DIR/venv" ]; then
    echo "[SETUP] Creating virtualenv and installing requirements..."
    python3 -m venv "$DIR/venv"
    "$DIR/venv/bin/pip" install --upgrade pip
    "$DIR/venv/bin/pip" install -r "$DIR/requirements.txt"
fi

# The tool needs root for direct V4L2/I2C and PWM sysfs access. It binds to
# localhost only (see app_EG.py), so it is not reachable from the network.
sudo "$DIR/venv/bin/python3" "$DIR/app_EG.py"
