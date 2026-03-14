"""
VESPA Live Tracker Viewer
Consumes ZMQ Debug frames from stereo_tracker.cpp, draws bounding boxes, and streams via Flask.
"""
from flask import Flask, render_template_string, Response
import zmq
import cv2
import numpy as np
import threading
import time
import json
import signal
import atexit
import os

app = Flask(__name__)

context = zmq.Context()
subscriber = context.socket(zmq.SUB)
# subscriber.setsockopt(zmq.CONFLATE, 1) # Drop old frames to ensure 0 latency
subscriber.connect("tcp://localhost:5556") # Connects to the tracker's debug port
subscriber.setsockopt(zmq.SUBSCRIBE, b"")

latest_frame = None
latest_targets = []
latest_lock = threading.Lock()
is_running = True

def zmq_receiver_thread():
    global latest_frame, latest_targets, is_running
    while is_running:
        parts = None
        
        # 1. Drain the socket manually to grab the absolute newest message in the queue
        while True:
            try:
                parts = subscriber.recv_multipart(flags=zmq.NOBLOCK)
            except zmq.Again:
                break # Queue is empty, we now hold the newest frame
            except zmq.ZMQError:
                is_running = False
                break
                
        # 2. Safely parse the newest multipart message
        if parts is not None and len(parts) == 2:
            with latest_lock:
                latest_frame = parts[0]
                try:
                    latest_targets = json.loads(parts[1].decode('utf-8'))
                except json.JSONDecodeError:
                    latest_targets = []
                    
        # Sleep 1ms to prevent pegging a CPU core at 100%
        time.sleep(0.001)

threading.Thread(target=zmq_receiver_thread, daemon=True).start()

def cleanup():
    global is_running
    is_running = False
    time.sleep(0.1)
    subscriber.close()
    context.term()
    print("[CLEANUP] Viewer shut down cleanly.")

atexit.register(cleanup)

def generate_stream():
    global latest_frame, latest_targets
    while True:
        with latest_lock:
            frame_data = latest_frame
            targets = latest_targets
        
        if frame_data is not None:
            # 1. Convert 2MB raw bytes
            nparr = np.frombuffer(frame_data, dtype=np.uint8)
            
            # 2. Split into Left and Right
            img_left_raw = nparr[:1280*800].reshape((800, 1280))
            img_right_raw = nparr[1280*800:].reshape((800, 1280))
            
            # 3. Rotate kinematically to upright (800x1280)
            img_left = cv2.rotate(img_left_raw, cv2.ROTATE_90_COUNTERCLOCKWISE)
            img_right = cv2.rotate(img_right_raw, cv2.ROTATE_90_COUNTERCLOCKWISE)
            
            # 4. Convert to Color 
            img_left_color = cv2.cvtColor(img_left, cv2.COLOR_GRAY2BGR)
            img_right_color = cv2.cvtColor(img_right, cv2.COLOR_GRAY2BGR)
            
            # 5. Glue them Side-By-Side FIRST (Result is 1600x1280)
            combined_img = cv2.hconcat([img_left_color, img_right_color])

            # 6. Draw Targets on the combined image (Prevents clipping boundary)
            for t in targets:
                cx, cy = int(t['cx']), int(t['cy'])
                
                # Draw the Crosshair
                cv2.drawMarker(combined_img, (cx, cy), (0, 255, 0), cv2.MARKER_CROSS, 40, 3)
                cv2.circle(combined_img, (cx, cy), 15, (0, 0, 255), 2)
                
                # Split the coordinates into a vertical list to utilize the vertical space
                lines = [
                    f"X: {int(t['x'])} mm",
                    f"Y: {int(t['y'])} mm",
                    f"Z: {int(t['z'])} mm"
                ]
                
                # Draw the stacked text 
                y_offset = cy - 40 # Start slightly above the crosshair
                for line in lines:
                    # Drawing on combined_img allows it to safely bleed into the right stream
                    cv2.putText(combined_img, line, (cx + 25, y_offset), 
                                cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 255), 2)
                    y_offset += 30 # Drop down for the next line

            # 7. Encode to Web Stream
            _, buffer = cv2.imencode('.jpg', combined_img, [cv2.IMWRITE_JPEG_QUALITY, 80])
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')
        else:
            time.sleep(0.01)

# Super simple single-file HTML frontend
HTML_PAGE = """
<!DOCTYPE html>
<html>
<head>
    <title>VESPA Live Tracker</title>
    <style>
        body { background: #111; color: #fff; text-align: center; font-family: sans-serif; }
        img { max-height: 80vh; border: 2px solid #555; }
    </style>
</head>
<body>
    <h2>VESPA Phase 3: Live 3D Tracking</h2>
    <img src="{{ url_for('video_feed') }}" alt="Stream Offline">
</body>
</html>
"""

@app.route('/')
def index(): return render_template_string(HTML_PAGE)
@app.route('/video_feed')
def video_feed(): return Response(generate_stream(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5001, debug=False, threaded=True) # Port 5001 so it doesn't collide with Calibration