from flask import Flask, render_template, request, jsonify
import sqlite3
import os

app = Flask(__name__)
DB_FILE = "orders.db"

# -------- DATABASE SETUP --------
def init_db():
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS orders (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                color TEXT NOT NULL,
                status TEXT DEFAULT 'PENDING'
            )
        ''')
        conn.commit()

# -------- WEB FRONTEND ROUTE --------
@app.route('/')
def index():
    return render_template('index.html')

# -------- API: ADD ORDER --------
@app.route('/api/add_order', methods=['POST'])
def add_order():
    data = request.json
    color = data.get('color')
    
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute("INSERT INTO orders (color, status) VALUES (?, 'PENDING')", (color,))
        conn.commit()
    return jsonify({"status": "success", "message": f"{color} added to queue."})

# -------- API: GET PENDING ORDERS --------
# The OpenCV script will call this to know what to look for
@app.route('/api/get_queue', methods=['GET'])
def get_queue():
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT id, color FROM orders WHERE status = 'PENDING'")
        rows = cursor.fetchall()
        
    # Format as a list of dictionaries for easy parsing
    queue = [{"id": row[0], "color": row[1]} for row in rows]
    return jsonify(queue)

# -------- API: COMPLETE ORDER --------
# The OpenCV script will call this when the Arduino says "DONE"
@app.route('/api/complete_order', methods=['POST'])
def complete_order():
    data = request.json
    order_id = data.get('id')
    
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute("UPDATE orders SET status = 'COMPLETED' WHERE id = ?", (order_id,))
        conn.commit()
    return jsonify({"status": "success"})

if __name__ == '__main__':
    init_db()
    # Run the server on port 5000, accessible to your local network
    app.run(host='0.0.0.0', port=5000, debug=True, threaded=True)  