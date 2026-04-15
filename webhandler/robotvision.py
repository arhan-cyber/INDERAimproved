import cv2
import numpy as np
import serial
import time
import requests # <--- NEW: For talking to the web server

# -------- CONFIGURATION --------
API_BASE_URL = "http://localhost:5000/api" # Pointing to our Flask app
PICK_X_CENTER = 320 
TOLERANCE = 30 
MIN_AREA = 1500 
REQUIRED_STABLE = 5 

# -------- SERIAL SETUP & HANDSHAKE --------
arduino = serial.Serial('COM6', 9600, timeout=1)  # change COM port
print("Waiting for Arduino to initialize...")
while True:
    if arduino.in_waiting:
        msg = arduino.readline().decode().strip()
        if msg == "SYSTEM READY":
            break
print("Handshake complete. Starting camera...")

cap = cv2.VideoCapture(0)

# -------- COLOR RANGES (HSV) --------
color_ranges = {
    "red1": ([0, 120, 70], [10, 255, 255]),
    "red2": ([170, 120, 70], [180, 255, 255]),
    "blue": ([100, 150, 50], [140, 255, 255]),
    "green": ([40, 70, 70], [80, 255, 255])
}

# -------- STATE --------
busy = False
last_detected = None
stable_count = 0
current_target_order = None # Store the dict: {"id": X, "color": "Y"}

# -------- HELPER FUNCTIONS --------
def get_pending_orders():
    try:
        response = requests.get(f"{API_BASE_URL}/get_queue")
        return response.json() # Returns a list of dicts like [{"id": 1, "color": "red"}]
    except:
        return []

def complete_order(order_id):
    try:
        requests.post(f"{API_BASE_URL}/complete_order", json={"id": order_id})
        print(f"Database updated: Order {order_id} completed.")
    except Exception as e:
        print(f"Failed to update database: {e}")

def send_command(cmd):
    global busy
    print(f"Sending to Arduino: {cmd}")
    arduino.write((cmd + "\n").encode())
    busy = True

# -------- MAIN LOOP --------
while True:
    ret, frame = cap.read()
    if not ret: break

    # 1. Fetch current queue from the database
    pending_orders = get_pending_orders()
    target_colors = {order['color'] for order in pending_orders} # Create a set of colors we are looking for

    # UI Guidelines
    cv2.line(frame, (PICK_X_CENTER - TOLERANCE, 0), (PICK_X_CENTER - TOLERANCE, frame.shape[0]), (0, 255, 255), 1)
    cv2.line(frame, (PICK_X_CENTER + TOLERANCE, 0), (PICK_X_CENTER + TOLERANCE, frame.shape[0]), (0, 255, 255), 1)

    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    detected_color = None
    in_pick_zone = False
    largest_area = 0

    # 2. Vision Processing
    for color_name, (lower, upper) in color_ranges.items():
        if color_name == "red2": continue

        mask = cv2.inRange(hsv, np.array(lower), np.array(upper))
        if color_name == "red1":
            lower2, upper2 = color_ranges["red2"]
            mask |= cv2.inRange(hsv, np.array(lower2), np.array(upper2))
            color_name = "red"

        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if contours:
            largest_cnt = max(contours, key=cv2.contourArea)
            area = cv2.contourArea(largest_cnt)

            if area > MIN_AREA and area > largest_area:
                largest_area = area
                x, y, w, h = cv2.boundingRect(largest_cnt)
                cx = x + w // 2
                
                cv2.rectangle(frame, (x, y), (x+w, y+h), (0,255,0), 2)
                cv2.putText(frame, color_name, (x, y-10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0,255,0), 2)

                if abs(cx - PICK_X_CENTER) <= TOLERANCE:
                    detected_color = color_name
                    in_pick_zone = True

    # 3. Stability Check
    if detected_color == last_detected and in_pick_zone:
        stable_count += 1
    else:
        stable_count = 0
    last_detected = detected_color if in_pick_zone else None

    # 4. Decision Making
    if detected_color is not None and stable_count >= REQUIRED_STABLE and not busy:
        if detected_color in target_colors:
            # Find the specific order ID for this color from the queue
            for order in pending_orders:
                if order['color'] == detected_color:
                    current_target_order = order
                    break
            
            print(f"Target {detected_color} recognized! Initiating pickup...")
            send_command("PICK")
        stable_count = 0 

    # 5. Serial Feedback & Database Update
    if arduino.in_waiting:
        msg = arduino.readline().decode().strip()
        print(f"Arduino: {msg}")

        if "ACTION: DONE" in msg:
            busy = False
            # The physical action finished! Tell the database to complete it.
            if current_target_order:
                complete_order(current_target_order['id'])
                current_target_order = None

    # 6. Display
    cv2.putText(frame, f"DB Targets: {target_colors}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255,255,255), 2)
    cv2.imshow("Robot Vision", frame)

    if cv2.waitKey(1) & 0xFF == 27: break

cap.release()
cv2.destroyAllWindows()
arduino.close()