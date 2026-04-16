import cv2
import numpy as np
import serial
import time
import requests
import threading # <-- Added for non-blocking HTTP requests

# -------- CONFIGURATION --------
API_BASE_URL = "http://localhost:5000/api" 
PICK_X_CENTER = 320 
TOLERANCE = 30 
MIN_AREA = 1500 
REQUIRED_STABLE = 5 

# -------- STACKING CONFIGURATION --------
color_stack_level = {
    "red": 1,
    "blue": 1,
    "green": 1
}

# -------- SERIAL SETUP & HANDSHAKE --------
# Reduced timeout from 0.1 to 0.01 to prevent readline() from bottlenecking FPS
arduino = serial.Serial('COM5', 9600, timeout=0.01)  
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
current_target_order = None 
last_untracked_color = None
untracked_logged = False

# ---- STATE FOR PERFORMANCE ----
last_api_check = 0
pending_orders = []
target_colors = set()

# -------- THREADED HELPER FUNCTIONS --------
# Running requests in threads prevents the cv2 window from freezing

def fetch_orders_thread():
    global pending_orders, target_colors
    try:
        response = requests.get(f"{API_BASE_URL}/get_queue", timeout=3)
        data = response.json()
        pending_orders = data
        target_colors = {order['color'] for order in data}
    except Exception as e:
        pass # If it fails, we just keep the existing pending_orders list

def complete_order_bg(order_id):
    try:
        requests.post(f"{API_BASE_URL}/complete_order", json={"id": order_id}, timeout=1)
        print(f"Database updated: Order {order_id} completed.")
    except Exception as e:
        print(f"Failed to update database: {e}")

def complete_order(order_id):
    # Fire and forget thread
    threading.Thread(target=complete_order_bg, args=(order_id,), daemon=True).start()

def add_to_inventory_bg(color):
    try:
        requests.post(
            f"{API_BASE_URL}/add_inventory",
            json={"color": color},
            timeout=1
        )
        print(f"Inventory updated: {color} block added.")
    except Exception as e:
        print(f"Failed to update inventory: {e}")

def add_to_inventory(color):
    # Fire and forget thread
    threading.Thread(target=add_to_inventory_bg, args=(color,), daemon=True).start()

# -------- HARDWARE HELPER FUNCTIONS --------
def send_command(cmd, level=1):
    global busy
    full_cmd = f"{cmd}:{level}"
    print(f"Sending to Arduino: {full_cmd}")
    arduino.write((full_cmd + "\n").encode())
    busy = True

def get_next_stack_level(color):
    level = color_stack_level[color]
    color_stack_level[color] = 2 if level == 1 else 1
    return level

def reset_stack_levels():
    global color_stack_level
    color_stack_level = {
        "red": 1,
        "blue": 1,
        "green": 1
    }
    arduino.write("RESET\n".encode())
    print("Stack levels reset")

# -------- MAIN LOOP --------
while True:
    ret, frame = cap.read()
    if not ret: break

    frame = cv2.resize(frame, (640, 480))

    # Check API every 0.5 seconds USING A BACKGROUND THREAD
    current_time = time.time()
    if current_time - last_api_check > 0.5:
        threading.Thread(target=fetch_orders_thread, daemon=True).start()
        last_api_check = current_time

    # UI Guidelines
    cv2.line(frame, (PICK_X_CENTER - TOLERANCE, 0), (PICK_X_CENTER - TOLERANCE, frame.shape[0]), (0, 255, 255), 1)
    cv2.line(frame, (PICK_X_CENTER + TOLERANCE, 0), (PICK_X_CENTER + TOLERANCE, frame.shape[0]), (0, 255, 255), 1)

    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    detected_color = None
    in_pick_zone = False
    largest_area = 0

    # Vision Processing
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
                
                # Display color and its next stack level
                next_level = color_stack_level[color_name]
                label = f"{color_name} (L{next_level})"
                cv2.putText(frame, label, (x, y-10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0,255,0), 2)

                if abs(cx - PICK_X_CENTER) <= TOLERANCE:
                    detected_color = color_name
                    in_pick_zone = True

    # Stability Check
    if detected_color == last_detected and in_pick_zone:
        stable_count += 1
    else:
        stable_count = 0
    
    if detected_color is None:
        untracked_logged = False
        last_untracked_color = None
        
    last_detected = detected_color if in_pick_zone else None

    # Decision Making
    if detected_color is not None and stable_count >= REQUIRED_STABLE and not busy:

        if detected_color in target_colors:
            # -------- EXISTING PICK LOGIC --------
            for order in pending_orders:
                if order['color'] == detected_color:
                    current_target_order = order
                    break
            
            stack_level = get_next_stack_level(detected_color)
            
            print(f"Target {detected_color} recognized! Stacking at Level {stack_level}")
            send_command("PICK", stack_level)

            # Do not force last_api_check = 0 here, let the interval handle it
            untracked_logged = False  # reset

        else:
            # -------- NEW INVENTORY LOGIC --------
            if detected_color != last_untracked_color or not untracked_logged:
                print(f"Untracked {detected_color} detected → logging to inventory")
                add_to_inventory(detected_color) # Now threaded!
                untracked_logged = True
                last_untracked_color = detected_color

        stable_count = 0

    # Serial Feedback & Database Update
    if arduino.in_waiting:
        msg = arduino.readline().decode().strip()
        if msg: # Only print if not empty
            print(f"Arduino: {msg}")

        if "ACTION: DONE" in msg:
            busy = False
            if current_target_order:
                complete_order(current_target_order['id']) # Now threaded!
                current_target_order = None

    # Display stack levels on screen
    y_offset = 30
    cv2.putText(frame, f"DB Targets: {list(target_colors)}", (10, y_offset), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255,255,255), 2)
    y_offset += 30
    
    for color, level in color_stack_level.items():
        level_text = f"{color}: Next->L{level}"
        cv2.putText(frame, level_text, (10, y_offset), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,0), 1)
        y_offset += 25
    
    cv2.imshow("Robot Vision", frame)

    key = cv2.waitKey(1) & 0xFF
    if key == 27:  # ESC to exit
        break
    elif key == ord('r'):  # Press 'r' to reset stack levels
        reset_stack_levels()

cap.release()
cv2.destroyAllWindows()
arduino.close()