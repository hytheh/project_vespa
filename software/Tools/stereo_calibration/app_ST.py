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
import signal
import atexit

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
latest_frame = None
latest_frame_lock = threading.Lock()
is_running = True

# THE LATENCY FIX: Drop old frames, only keep the newest one in the buffer
subscriber.setsockopt(zmq.CONFLATE, 1) 

subscriber.connect("tcp://localhost:5555")
subscriber.setsockopt(zmq.SUBSCRIBE, b"")

latest_frame = None
latest_frame_lock = threading.Lock()

def zmq_receiver_thread():
    """Continuously pulls ZMQ frames to prevent buffer lag."""
    global latest_frame, is_running
    while is_running: # <-- Replaced 'while True'
        try:
            msg = subscriber.recv(flags=zmq.NOBLOCK)
            with latest_frame_lock:
                latest_frame = msg
        except zmq.Again:
            time.sleep(0.001)
        except zmq.ZMQError:
            break # Catch if the socket closes while we are waiting

threading.Thread(target=zmq_receiver_thread, daemon=True).start()

# --- GRACEFUL SHUTDOWN HANDLER ---
def cleanup_network():
    """At-Exit hook to safely release ZMQ sockets."""
    global is_running
    print("\n[CLEANUP] Halting ZMQ receiver thread...")
    is_running = False # Tell the thread to stop
    time.sleep(0.1)    # Give it 100ms to exit the loop gracefully
    
    print("[CLEANUP] Closing ZMQ sockets and terminating Flask...")
    try:
        subscriber.close()
        context.term()
    except Exception:
        pass
    print("[CLEANUP] VESPA Stereo Calibrator exits cleanly. Goodbye!")

atexit.register(cleanup_network)

# --- Flask Routes ---
@app.route('/')
def index():
    return render_template('index_ST.html')

def generate_stream():
    """Yields the decoded, mathematically rotated MJPEG stream to the web UI."""
    global latest_frame
    while True:
        with latest_frame_lock:
            frame_data = latest_frame
        
        if frame_data is not None:
            nparr = np.frombuffer(frame_data, np.uint8)
            combined_img = cv2.imdecode(nparr, cv2.IMREAD_GRAYSCALE)
            
            h, w = combined_img.shape
            half_w = w // 2
            img_left = combined_img[:, :half_w]
            img_right = combined_img[:, half_w:]
            
            # Rotate both halves individually (Counter-Clockwise for Kinematics)
            img_left = cv2.rotate(img_left, cv2.ROTATE_90_COUNTERCLOCKWISE)
            img_right = cv2.rotate(img_right, cv2.ROTATE_90_COUNTERCLOCKWISE)
            
            # Re-glue them side-by-side (Result is 1600x1280)
            display_img = cv2.hconcat([img_left, img_right])
            
            _, buffer = cv2.imencode('.jpg', display_img, [cv2.IMWRITE_JPEG_QUALITY, 80])
            
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')
        else:
            time.sleep(0.01)

@app.route('/video_feed')
def video_feed():
    return Response(generate_stream(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/capture', methods=['POST'])
def capture_pair():
    """Captures a frame, rotates it counter-clockwise, and finds corners."""
    global image_size
    
    with latest_frame_lock:
        if latest_frame is None:
            return jsonify({"status": "error", "message": "No stream data."})
        frame_data = latest_frame

    nparr = np.frombuffer(frame_data, np.uint8)
    combined_img = cv2.imdecode(nparr, cv2.IMREAD_GRAYSCALE)
    if combined_img is None:
        return jsonify({"status": "error", "message": "Decode failed."})

    h, w = combined_img.shape
    half_w = w // 2
    img_left = combined_img[:, :half_w]
    img_right = combined_img[:, half_w:]

    img_left = cv2.rotate(img_left, cv2.ROTATE_90_COUNTERCLOCKWISE)
    img_right = cv2.rotate(img_right, cv2.ROTATE_90_COUNTERCLOCKWISE)

    if image_size is None:
        image_size = (img_left.shape[1], img_left.shape[0])

    ret_l, corners_l = cv2.findChessboardCorners(img_left, CHESSBOARD_SIZE, None)
    ret_r, corners_r = cv2.findChessboardCorners(img_right, CHESSBOARD_SIZE, None)

    if ret_l and ret_r:
        criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
        corners_l = cv2.cornerSubPix(img_left, corners_l, (5, 5), (-1, -1), criteria)
        corners_r = cv2.cornerSubPix(img_right, corners_r, (5, 5), (-1, -1), criteria)

        objpoints.append(objp)
        imgpoints_left.append(corners_l)
        imgpoints_right.append(corners_r)
        
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
    flags = cv2.CALIB_FIX_INTRINSIC
    criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 100, 1e-5)
    
    ret_stereo, _, _, _, _, R, T, E, F = cv2.stereoCalibrate(
        objpoints, imgpoints_left, imgpoints_right,
        mtx_l, dist_l, mtx_r, dist_r,
        image_size, criteria=criteria, flags=flags)

    calibration_data = {
        "rms_error": ret_stereo,
        "camera_matrix_left": mtx_l.tolist(),
        "dist_coeff_left": dist_l.tolist(),
        "camera_matrix_right": mtx_r.tolist(),
        "dist_coeff_right": dist_r.tolist(),
        "R": R.tolist(), 
        "T": T.tolist()  
    }

    config_path = "/home/hytheh/project_vespa/software/vision_node/config/stereovision_settings.json"
    os.makedirs(os.path.dirname(config_path), exist_ok=True)
    with open(config_path, 'w') as f:
        json.dump(calibration_data, f, indent=4)

    # Extract the horizontal physical distance between the cameras in mm
    # T is a 3x1 vector. T[0][0] is the X-axis translation.
    baseline_mm = abs(T[0][0]) 
    
    # Extract Focal Length (Fx) and convert from pixels to mm (OV9281 = 0.003mm pixel size)
    focal_length_px = mtx_l[0][0]
    focal_length_mm = focal_length_px * 0.003

    return jsonify({
        "status": "success", 
        "rms": round(ret_stereo, 3), 
        "baseline": round(baseline_mm, 2), 
        "focal_length_mm": round(focal_length_mm, 2), # <-- Pass it as mm
        "pairs_used": len(objpoints)
    })

    return jsonify({
        "status": "success", 
        "rms": round(ret_stereo, 3), 
        "baseline": round(baseline_mm, 2), 
        "focal_length_px": round(focal_length_px, 2), # <-- Pass it to the frontend
        "pairs_used": len(objpoints)
    })

@app.route('/shutdown', methods=['POST'])
def shutdown():
    """Receives the UI shutdown command."""
    def delayed_kill():
        time.sleep(0.5)
        os.kill(os.getpid(), signal.SIGINT)
    
    threading.Thread(target=delayed_kill).start()
    return jsonify({"status": "shutting down"})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)