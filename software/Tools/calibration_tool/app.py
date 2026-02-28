import os
import subprocess
import json
import cv2
import numpy as np
from flask import Flask, render_template, Response, request, jsonify
import signal
import sys
import time

app = Flask(__name__)

# --- Configuration Paths ---
PWM_START_CMD = "sudo /home/hytheh/project_vespa/software/vision_node/PWM_sync_start.sh"
PWM_STOP_CMD = "sudo /home/hytheh/project_vespa/software/vision_node/PWM_sync_stop.sh"
MAIN_CONFIG_PATH = "/home/hytheh/project_vespa/software/vision_node/config/camera_settings.json"
ACTIVE_PROFILE_PATH = "/home/hytheh/project_vespa/software/vision_node/config/.active_profile"
PROFILES_DIR = "/home/hytheh/project_vespa/software/vision_node/config/profiles"

# V4L2 Control mappings
CONTROLS = {
    "exposure": "0x00980911",           
    "analogue_gain": "0x009e0903",      
    "horizontal_flip": "0x00980914",    
    "vertical_flip": "0x00980915"       
}

DEFAULT_CONFIG = {
    "camera0": {"exposure": 681, "analogue_gain": 100, "horizontal_flip": 0, "vertical_flip": 0},
    "camera1": {"exposure": 681, "analogue_gain": 100, "horizontal_flip": 0, "vertical_flip": 0}
}

# --- Hardware Orchestration ---
def start_pwm():
    print("[INFO] Starting PWM Sync...")
    subprocess.run(PWM_START_CMD, shell=True)

def stop_pwm():
    print("\n[INFO] Stopping PWM Sync...")
    subprocess.run(PWM_STOP_CMD, shell=True)

def apply_trigger_mode(dev):
    subprocess.run(f"v4l2-ctl -d {dev} -c 0x00981902=1", shell=True)
    subprocess.run(f"v4l2-ctl -d {dev} -c 0x00981903=12000", shell=True)
    subprocess.run(f"v4l2-ctl -d {dev} -c 0x00981901=1", shell=True)

os.makedirs(PROFILES_DIR, exist_ok=True)
start_pwm()
apply_trigger_mode("/dev/video0")
apply_trigger_mode("/dev/video1")

cap0 = cv2.VideoCapture(0, cv2.CAP_V4L2)
cap1 = cv2.VideoCapture(1, cv2.CAP_V4L2)

is_running = True

# --- Graceful Shutdown Handler ---
def signal_handler(sig, frame):
    global is_running
    print("\n[INFO] Shutting down calibration tool safely...")
    is_running = False  
    
    print("[INFO] Releasing V4L2 camera devices...")
    if cap0.isOpened(): cap0.release()
    if cap1.isOpened(): cap1.release()
        
    stop_pwm()
    print("[INFO] Exited cleanly.")
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

def set_v4l2_ctrl(dev_id, ctrl_name, value):
    ctrl_id = CONTROLS.get(ctrl_name)
    if ctrl_id:
        cmd = f"v4l2-ctl -d /dev/video{dev_id} -c {ctrl_id}={value}"
        subprocess.run(cmd, shell=True)

def draw_histogram(frame, channel_name):
    hist = cv2.calcHist([frame], [0], None, [256], [0, 256])
    cv2.normalize(hist, hist, 0, 100, cv2.NORM_MINMAX)
    overlay = np.zeros((120, 276), dtype=np.uint8)
    for x, y in enumerate(hist):
        cv2.line(overlay, (x+10, 110), (x+10, 110 - int(y[0])), (255), 1)
    cv2.putText(overlay, f"{channel_name} Hist", (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255), 1)
    frame[10:130, 10:286] = overlay
    return frame

def generate_frames():
    global is_running
    while is_running:
        ret0, frame0 = cap0.read()
        ret1, frame1 = cap1.read()

        if not ret0 or not ret1:
            time.sleep(0.01)
            continue

        if len(frame0.shape) == 3:
            frame0 = cv2.cvtColor(frame0, cv2.COLOR_BGR2GRAY)
            frame1 = cv2.cvtColor(frame1, cv2.COLOR_BGR2GRAY)

        frame0 = cv2.rotate(frame0, cv2.ROTATE_90_CLOCKWISE)
        frame1 = cv2.rotate(frame1, cv2.ROTATE_90_CLOCKWISE)

        mse = np.mean((frame0.astype("float") - frame1.astype("float")) ** 2)
        
        frame0 = draw_histogram(frame0, "Cam 0")
        frame1 = draw_histogram(frame1, "Cam 1")

        combined = np.hstack((frame0, frame1))
        combined = cv2.resize(combined, (1000, 800))
        
        cv2.putText(combined, f"Disparity (MSE): {mse:.2f} (Lower = Better)", 
                    (combined.shape[1]//2 - 200, combined.shape[0] - 30), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255), 2)

        ret, buffer = cv2.imencode('.jpg', combined)
        frame = buffer.tobytes()

        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

# --- Routes ---
@app.route('/')
def index():
    return render_template('index.html')

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/update_control', methods=['POST'])
def update_control():
    data = request.json
    set_v4l2_ctrl(data['cam_id'], data['control'], data['value'])
    return jsonify({"status": "success"})

@app.route('/apply_full_config', methods=['POST'])
def apply_full_config():
    data = request.json
    for cam_id in [0, 1]:
        conf = data[f'camera{cam_id}']
        cmd = f"v4l2-ctl -d /dev/video{cam_id} "
        cmd += f"-c {CONTROLS['exposure']}={conf['exposure']} "
        cmd += f"-c {CONTROLS['analogue_gain']}={conf['analogue_gain']} "
        cmd += f"-c {CONTROLS['horizontal_flip']}={conf['horizontal_flip']} "
        cmd += f"-c {CONTROLS['vertical_flip']}={conf['vertical_flip']}"
        subprocess.run(cmd, shell=True)
        time.sleep(0.05) 
    return jsonify({"status": "success"})

@app.route('/list_profiles', methods=['GET'])
def list_profiles():
    profiles = ["default"]
    for f in os.listdir(PROFILES_DIR):
        if f.endswith('.json'):
            profiles.append(f.replace('.json', ''))
            
    # Read the last active profile, default to 'default' if tracking file is missing
    active_profile = "default"
    if os.path.exists(ACTIVE_PROFILE_PATH):
        with open(ACTIVE_PROFILE_PATH, 'r') as f:
            saved = f.read().strip()
            if saved in profiles:
                active_profile = saved

    return jsonify({"profiles": profiles, "active": active_profile})

@app.route('/create_profile', methods=['POST'])
def create_profile():
    name = request.json.get('name')
    if not name or name == "default":
        return jsonify({"error": "Invalid or reserved name"}), 400
    path = os.path.join(PROFILES_DIR, f"{name}.json")
    with open(path, 'w') as f:
        json.dump(DEFAULT_CONFIG, f, indent=4)
    return jsonify({"status": "success", "name": name})

@app.route('/load_profile/<name>', methods=['GET'])
def load_profile(name):
    if name == "default":
        return jsonify(DEFAULT_CONFIG)
    path = os.path.join(PROFILES_DIR, f"{name}.json")
    if os.path.exists(path):
        with open(path, 'r') as f:
            return jsonify(json.load(f))
    return jsonify(DEFAULT_CONFIG)

@app.route('/apply_and_save', methods=['POST'])
def apply_and_save():
    data = request.json
    profile_name = data.get('profile_name')
    config_data = data.get('config')

    if profile_name != "default":
        profile_path = os.path.join(PROFILES_DIR, f"{profile_name}.json")
        with open(profile_path, 'w') as f:
            json.dump(config_data, f, indent=4)

    os.makedirs(os.path.dirname(MAIN_CONFIG_PATH), exist_ok=True)
    with open(MAIN_CONFIG_PATH, 'w') as f:
        json.dump(config_data, f, indent=4)
        
    # Update the tracking file
    with open(ACTIVE_PROFILE_PATH, 'w') as f:
        f.write(profile_name)

    return jsonify({"status": "success"})

@app.route('/shutdown', methods=['POST'])
def shutdown():
    print("[INFO] Shutdown requested via web interface.")
    # Send SIGINT to own process to trigger the clean signal_handler
    os.kill(os.getpid(), signal.SIGINT)
    return jsonify({"status": "shutting down"})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, threaded=True)