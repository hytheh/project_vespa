"""
VESPA Live Stereo Calibrator - Main Backend
Receives ZMQ 60Hz pairs, extracts checkerboard corners, and computes stereo matrices.
"""
from flask import Flask, render_template, Response, jsonify
import zmq
import cv2
import numpy as np
import threading
import time
import json
import os

app = Flask(__name__)

# --- Calibration Constants ---
CHESSBOARD_SIZE = (9, 6)
SQUARE_SIZE_MM = 15.0

# Prepare the physical 3D coordinates of the board (Z=0)
objp = np.zeros((CHESSBOARD_SIZE[0] * CHESSBOARD_SIZE[1], 3), np.float32)
objp[:, :2] = np.mgrid[0:CHESSBOARD_SIZE[0], 0:CHESSBOARD_SIZE[1]].T.reshape(-1, 2)
objp *= SQUARE_SIZE_MM

# Memory for captured valid pairs
objpoints = []       # 3d points in real world space
imgpoints_left = []  # 2d points in image plane (Left)
imgpoints_right = [] # 2d points in image plane (Right)
image_size = None    # Will be captured on the first frame

# --- ZMQ Stream Receiver ---
context = zmq.Context()
subscriber = context.socket(zmq.SUB)
subscriber.connect("tcp://localhost:5555")
subscriber.setsockopt(zmq.SUBSCRIBE, b"")

latest_frame = None
latest_frame_lock = threading.Lock()

def zmq_receiver_thread():
    """Continuously pulls ZMQ frames to prevent buffer lag."""
    global latest_frame
    while True:
        try:
            msg = subscriber.recv(flags=zmq.NOBLOCK)
            with latest_frame_lock:
                latest_frame = msg
        except zmq.Again:
            time.sleep(0.001)

# Start background ZMQ receiver
threading.Thread(target=zmq_receiver_thread, daemon=True).start()

# --- Flask Routes ---
@app.route('/')
def index():
    return render_template('index_ST.html')

def generate_stream():
    """Yields the ZMQ MJPEG stream to the web UI."""
    global latest_frame
    while True:
        with latest_frame_lock:
            frame_data = latest_frame
        
        if frame_data is not None:
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame_data + b'\r\n')
        else:
            time.sleep(0.01)

@app.route('/video_feed')
def video_feed():
    return Response(generate_stream(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/capture', methods=['POST'])
def capture_pair():
    """Grabs the current frame, splits it, and looks for the checkerboard."""
    global latest_frame, image_size

    with latest_frame_lock:
        if latest_frame is None:
            return jsonify({"status": "error", "message": "No stream data."})
        frame_data = latest_frame

    # Decode the JPEG back into an OpenCV matrix
    nparr = np.frombuffer(frame_data, np.uint8)
    combined_img = cv2.imdecode(nparr, cv2.IMREAD_GRAYSCALE)

    # Split the concatenated 60Hz frame into Left and Right
    h, w = combined_img.shape
    half_w = w // 2
    img_left = combined_img[:, :half_w]
    img_right = combined_img[:, half_w:]

    if image_size is None:
        image_size = (half_w, h)

    # Find the chessboard corners
    ret_l, corners_l = cv2.findChessboardCorners(img_left, CHESSBOARD_SIZE, None)
    ret_r, corners_r = cv2.findChessboardCorners(img_right, CHESSBOARD_SIZE, None)

    if ret_l and ret_r:
        # Refine corner locations to sub-pixel accuracy
        criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
        corners2_l = cv2.cornerSubPix(img_left, corners_l, (11, 11), (-1, -1), criteria)
        corners2_r = cv2.cornerSubPix(img_right, corners_r, (11, 11), (-1, -1), criteria)

        objpoints.append(objp)
        imgpoints_left.append(corners2_l)
        imgpoints_right.append(corners2_r)

        return jsonify({"status": "success", "count": len(objpoints)})
    else:
        return jsonify({"status": "failed", "message": "Board not fully visible in BOTH cameras."})

@app.route('/compute', methods=['POST'])
def compute_calibration():
    """Executes the stereo calibration math and saves the matrices."""
    if len(objpoints) < 10:
        return jsonify({"status": "error", "message": f"Need at least 10 pairs. You have {len(objpoints)}."})

    print("[CALIBRATION] Calibrating Left Camera...")
    ret_l, mtx_l, dist_l, _, _ = cv2.calibrateCamera(objpoints, imgpoints_left, image_size, None, None)

    print("[CALIBRATION] Calibrating Right Camera...")
    ret_r, mtx_r, dist_r, _, _ = cv2.calibrateCamera(objpoints, imgpoints_right, image_size, None, None)

    print("[CALIBRATION] Running Stereo Calibration Matrix Math...")
    # Lock the individual camera matrices and only compute the physical translation/rotation between them
    flags = cv2.CALIB_FIX_INTRINSIC
    criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 100, 1e-5)
    
    ret_stereo, _, _, _, _, R, T, E, F = cv2.stereoCalibrate(
        objpoints, imgpoints_left, imgpoints_right,
        mtx_l, dist_l, mtx_r, dist_r,
        image_size, criteria=criteria, flags=flags)

    # Save to JSON
    calibration_data = {
        "rms_error": ret_stereo,
        "camera_matrix_left": mtx_l.tolist(),
        "dist_coeff_left": dist_l.tolist(),
        "camera_matrix_right": mtx_r.tolist(),
        "dist_coeff_right": dist_r.tolist(),
        "R": R.tolist(), # Rotation matrix between cameras
        "T": T.tolist()  # Translation vector (Baseline physical distance in mm)
    }

    config_path = "/home/hytheh/project_vespa/software/vision_node/config/stereovision_settings.json"
    os.makedirs(os.path.dirname(config_path), exist_ok=True)
    with open(config_path, 'w') as f:
        json.dump(calibration_data, f, indent=4)

    return jsonify({"status": "success", "rms": round(ret_stereo, 3), "pairs_used": len(objpoints)})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)