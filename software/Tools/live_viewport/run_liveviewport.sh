#!/bin/bash

# Navigate to the directory where the script is located
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
VENV_DIR="$DIR/venv_liveview"

echo "=== VESPA Live Tracker Viewport ==="

# 1. Create the virtual environment if it doesn't exist
if [ ! -d "$VENV_DIR" ]; then
    echo "[SETUP] Creating isolated Python virtual environment..."
    python3 -m venv "$VENV_DIR"
fi

# 2. Activate the virtual environment
source "$VENV_DIR/bin/activate"

# 3. Install required dependencies
# Using -q (quiet) to keep the terminal output clean after the first setup
echo "[SETUP] Verifying dependencies (Flask, pyzmq, opencv-python, numpy)..."
pip install --upgrade pip -q
pip install Flask pyzmq opencv-python numpy -q

# 4. Run the Python application
echo "[LAUNCH] Starting Flask Viewport Server on port 5001..."
python3 "$DIR/app_TR.py"

# 5. Cleanup when the user exits (Ctrl+C)
echo "[CLEANUP] Deactivating virtual environment..."
deactivate