#!/bin/bash
# VESPA Stereo Calibrator - Auto-Environment Wrapper

# Get the absolute directory where this script is located
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

echo "[LAUNCHER] Starting VESPA Stereo Calibrator..."

# Create the virtualenv and install dependencies on first run.
if [ ! -d "$DIR/venv" ]; then
    echo "[SETUP] Creating virtualenv and installing requirements..."
    python3 -m venv "$DIR/venv"
    "$DIR/venv/bin/pip" install --upgrade pip
    "$DIR/venv/bin/pip" install -r "$DIR/requirements.txt"
fi

# NOTE: this UI consumes the ZMQ stream from the C++ calibration_bridge, which
# must be running separately (it holds the hardware lock). This tool binds to
# localhost only (see app_ST.py).
"$DIR/venv/bin/python3" "$DIR/app_ST.py"
