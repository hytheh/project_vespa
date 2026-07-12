"""!
@file app_EG.py
@brief VESPA Exposure & Gain Calibrator Backend
@details A Flask-based web server and zero-latency OpenCV streaming engine.
Handles stereoscopic V4L2 hardware control, bypassing Tegra driver caching bugs
to ensure deterministic hardware states.

@author Project V.E.S.P.A. Team
@version 8.1 (Production Polish - Physical Units & UI Sync)
"""

from flask import Flask, render_template, request, jsonify, Response
import json
import subprocess
import os
import cv2
import numpy as np
import threading
import time
import signal
import atexit

app = Flask(__name__)

# --- Configuration Paths ---
MAIN_CONFIG_PATH = "/home/hytheh/project_vespa/software/vision_node/config/camera_settings.json"
ACTIVE_PROFILE_PATH = "/home/hytheh/project_vespa/software/vision_node/config/.active_profile"
PROFILES_DIR = "/home/hytheh/project_vespa/software/vision_node/config/profiles"

DEFAULT_CONFIG = {
    "camera0": {"exposure": 681, "analogue_gain": 100, "horizontal_flip": 0, "vertical_flip": 0},
    "camera1": {"exposure": 681, "analogue_gain": 100, "horizontal_flip": 0, "vertical_flip": 0}
}

os.makedirs(PROFILES_DIR, exist_ok=True)

# --- PHYSICAL DEVICE MAPPING ---
# Note: Physically mounted opposite to logical numbering. 
DEVICE_MAP = {
    "camera0": 1, # Right Eye
    "camera1": 0  # Left Eye
}

def clamp_pwm_hardware():
    """!
    @brief Safely disables Jetson PWM hardware to prevent sensor triggering conflicts.
    """
    print("[INIT] Securing Jetson PWM hardware...")
    subprocess.run("echo 0 > /sys/class/pwm/pwmchip3/pwm0/enable 2>/dev/null", shell=True)
    subprocess.run("echo 0 > /sys/class/pwm/pwmchip3/pwm0/duty_cycle 2>/dev/null", shell=True)

class CameraStream:
    """!
    @brief Manages a zero-latency V4L2 OpenCV video stream.
    @details Spawns a daemon thread to constantly pull frames, bypassing OpenCV's internal buffers.
    """
    def __init__(self, logical_name, device_id):
        self.logical_name = logical_name
        self.device_id = device_id
        self.cap = None
        self.frame = None
        self.running = False
        self.new_frame_event = threading.Event() 
        self.thread = threading.Thread(target=self.update, daemon=True)

    def start(self):
        # Force Master Mode (Free-run) for calibration
        subprocess.run(f"v4l2-ctl -d /dev/video{self.device_id} -c trigger_mode=0", shell=True)
        self.cap = cv2.VideoCapture(self.device_id, cv2.CAP_V4L2)
        self.cap.set(cv2.CAP_PROP_FPS, 60)
        self.cap.set(cv2.CAP_PROP_BUFFERSIZE, 1) 
        self.running = True
        self.thread.start()

    def stop(self):
        self.running = False
        if self.cap: self.cap.release()

    def draw_histogram(self, frame):
        hist = cv2.calcHist([frame], [0], None, [256], [0, 256])
        cv2.normalize(hist, hist, 0, 100, cv2.NORM_MINMAX)
        overlay = np.zeros((120, 276), dtype=np.uint8)
        for x, y in enumerate(hist):
            cv2.line(overlay, (x+10, 110), (x+10, 110 - int(y[0])), (255), 1)
        cv2.putText(overlay, f"{self.logical_name} Hist", (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255), 1)
        frame[10:130, 10:286] = overlay
        return frame

    def update(self):
        while self.running and self.cap.isOpened():
            ret, frame = self.cap.read()
            if ret:
                if len(frame.shape) == 3:
                    frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                
                # Portrait mounting correction: Sideways sensors must be rotated upright
                frame = cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
                
                self.frame = self.draw_histogram(frame)
                self.new_frame_event.set() 
            else:
                time.sleep(0.001)

    def get_encoded_frame(self):
        if self.new_frame_event.wait(timeout=1.0):
            self.new_frame_event.clear()
            if self.frame is not None:
                _, buffer = cv2.imencode('.jpg', self.frame, [cv2.IMWRITE_JPEG_QUALITY, 80])
                return buffer.tobytes()
        return b'\x00'

# Instantiate Global Streams
cam0 = CameraStream("Cam 0", DEVICE_MAP["camera0"])
cam1 = CameraStream("Cam 1", DEVICE_MAP["camera1"])

# --- GRACEFUL SHUTDOWN HANDLER ---
def cleanup_hardware():
    """!
    @brief At-Exit hook to safely release video nodes and hardware locks.
    @details Ensures clean exit whether triggered via UI or Ctrl+C SIGINT.
    """
    print("\n[CLEANUP] Safely releasing V4L2 hardware and terminating threads...")
    cam0.stop()
    cam1.stop()
    clamp_pwm_hardware()
    print("[CLEANUP] VESPA exits cleanly. Goodbye!")

atexit.register(cleanup_hardware)

def execute_v4l2_set(dev_id, ctrl_name, value):
    """!
    @brief Executes an atomic V4L2 I2C command via system subprocess.
    @param dev_id The /dev/videoX node ID.
    @param ctrl_name The string name of the V4L2 control.
    @param value The integer or boolean value to apply.
    """
    try:
        if isinstance(value, bool): value = 1 if value else 0
        cmd = f"v4l2-ctl -d /dev/video{dev_id} -c {ctrl_name}={value}"
        subprocess.run(cmd, shell=True)
        # 50ms delay to prevent Tegra driver from dropping sequential commands
        time.sleep(0.05) 
    except Exception as e:
        print(f"[ERROR] Set failed: {e}")

# --- FLASK ROUTES ---

@app.route('/initialize_cameras')
def initialize_cameras():
    if not cam0.running: cam0.start()
    if not cam1.running: cam1.start()
    return jsonify({"status": "initialized"})

@app.route('/command', methods=['POST'])
def apply_command():
    data = request.json
    logical_cam = data.get('camera_id') 
    dev_id = DEVICE_MAP[logical_cam]     
    command_key = data.get('command')
    
    # Translate UI shorthand to strict V4L2 nomenclature
    if command_key == 'gain':
        command_key = 'analogue_gain'
        
    value = data.get('value')
    execute_v4l2_set(dev_id, command_key, value)
    return jsonify({"status": "success"})

@app.route('/get_mse')
def get_mse():
    if cam0.frame is not None and cam1.frame is not None and cam0.frame.shape == cam1.frame.shape:
        mse = np.mean((cam0.frame.astype("float") - cam1.frame.astype("float")) ** 2)
        return jsonify({"mse": round(mse, 2)})
    return jsonify({"mse": "Calculating..."})

def gen(camera):
    while True:
        frame = camera.get_encoded_frame()
        if frame != b'\x00':
            yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
        else:
            time.sleep(0.01)

@app.route('/video_feed/<cam_id>')
def video_feed(cam_id):
    cam = cam0 if cam_id == '0' else cam1
    return Response(gen(cam), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/list_profiles', methods=['GET'])
def list_profiles():
    profiles = ["default"]
    for f in os.listdir(PROFILES_DIR):
        if f.endswith('.json'): profiles.append(f.replace('.json', ''))
    active_profile = "default"
    if os.path.exists(ACTIVE_PROFILE_PATH):
        with open(ACTIVE_PROFILE_PATH, 'r') as f:
            saved = f.read().strip()
            if saved in profiles: active_profile = saved
    return jsonify({"profiles": profiles, "active": active_profile})

@app.route('/create_profile', methods=['POST'])
def create_profile():
    name = request.json.get('name')
    if not name or name == "default": return jsonify({"error": "Invalid name"}), 400
    path = os.path.join(PROFILES_DIR, f"{name}.json")
    with open(path, 'w') as f: json.dump(DEFAULT_CONFIG, f, indent=4)
    return jsonify({"status": "success", "name": name})

@app.route('/load_profile/<name>', methods=['GET'])
def load_profile(name):
    if name == "default": return jsonify(DEFAULT_CONFIG)
    path = os.path.join(PROFILES_DIR, f"{name}.json")
    if os.path.exists(path):
        with open(path, 'r') as f: return jsonify(json.load(f))
    return jsonify(DEFAULT_CONFIG)

@app.route('/apply_full_config', methods=['POST'])
def apply_full_config():
    """!
    @brief Pushes an entire JSON profile to the hardware.
    @details Implements an 'Automated Jiggle' to defeat the Tegra V4L2 caching bug.
    The driver inherently ignores '0' commands on boot. We explicitly force state '1', 
    wait for the I2C bus to sync, and then apply the target config.
    """
    data = request.json
    for logical_cam in ["camera0", "camera1"]:
        conf = data[logical_cam]
        dev_id = DEVICE_MAP[logical_cam]
        
        # 1. THE AUTOMATED JIGGLE: Force V4L2 cache to synchronize
        execute_v4l2_set(dev_id, 'horizontal_flip', 1)
        execute_v4l2_set(dev_id, 'vertical_flip', 1)
        
        # Give the bus a 200ms window to clear the dummy commands
        time.sleep(0.2) 
        
        # 2. APPLY TARGET CONFIG
        execute_v4l2_set(dev_id, 'horizontal_flip', conf['horizontal_flip'])
        execute_v4l2_set(dev_id, 'vertical_flip', conf['vertical_flip'])
        execute_v4l2_set(dev_id, 'exposure', conf['exposure'])
        execute_v4l2_set(dev_id, 'analogue_gain', conf['analogue_gain'])
        
    return jsonify({"status": "success"})

@app.route('/apply_and_save', methods=['POST'])
def apply_and_save():
    data = request.json
    profile_name = data.get('profile_name')
    config_data = data.get('config')
    if profile_name != "default":
        with open(os.path.join(PROFILES_DIR, f"{profile_name}.json"), 'w') as f:
            json.dump(config_data, f, indent=4)
    os.makedirs(os.path.dirname(MAIN_CONFIG_PATH), exist_ok=True)
    with open(MAIN_CONFIG_PATH, 'w') as f: json.dump(config_data, f, indent=4)
    with open(ACTIVE_PROFILE_PATH, 'w') as f: f.write(profile_name)
    return jsonify({"status": "success"})

@app.route('/shutdown', methods=['POST'])
def shutdown():
    """!
    @brief Receives the UI shutdown command.
    @details Delays the internal SIGINT by 0.5s so the HTTP 200 OK reaches the browser.
    """
    def delayed_kill():
        time.sleep(0.5)
        os.kill(os.getpid(), signal.SIGINT)
    
    threading.Thread(target=delayed_kill).start()
    return jsonify({"status": "shutting down"})

@app.route('/')
def index():
    return render_template('index_EG.html')

if __name__ == '__main__':
    clamp_pwm_hardware()
    app.run(host='0.0.0.0', port=5000, debug=False)