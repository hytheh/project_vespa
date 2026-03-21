#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
VENV_DIR="$DIR/venv_liveview"
BUILD_DIR="$(realpath "$DIR/../../vision_node/build")"

echo "=== VESPA Unified Viewport Launcher ==="

# 1. Ask for sudo upfront
sudo -v

# 2. Define graceful shutdown behavior (Now triggered on EXIT)
cleanup() {
    echo -e "\n[CLEANUP] Stopping Flask Viewport..."
    # Assassinate the background Python UI
    kill -9 $PYTHON_PID 2>/dev/null
    deactivate 2>/dev/null
    echo "[CLEANUP] Teardown complete. Goodbye."
}
# Execute cleanup when the script naturally finishes or is killed
trap cleanup EXIT

# 3. Compile
echo "[SETUP] Ensuring C++ node is compiled with DEBUG_MODE..."
cd "$BUILD_DIR"
cmake -DDEBUG_MODE=ON .. > /dev/null
make stereo_tracker -j4 > /dev/null

# 4. Start the Python UI in the BACKGROUND
echo "[SETUP] Verifying Python environment..."
cd "$DIR"
if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv "$VENV_DIR"
fi
source "$VENV_DIR/bin/activate"

echo "[LAUNCH] Starting Flask Viewport Server in the background..."
python3 "$DIR/app_TR.py" &
PYTHON_PID=$!

# Give Python 1.5 seconds to boot the Flask server
sleep 1.5

# 5. Start the C++ Tracker in the FOREGROUND
echo "[LAUNCH] Starting C++ Stereo Tracker in the foreground..."
# Navigate back to the build directory where the executable actually lives!
cd "$BUILD_DIR"
# Because this is in the foreground, it owns the terminal.
sudo ./stereo_tracker