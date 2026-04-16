from flask import Flask, render_template, request, jsonify
import sqlite3

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

        # Added UNIQUE constraint to color for UPSERT compatibility
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS inventory (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                color TEXT UNIQUE NOT NULL,
                count INTEGER DEFAULT 1
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
    
    with sqlite3.connect(DB_FILE, timeout=5) as conn:
        cursor = conn.cursor()
        cursor.execute("INSERT INTO orders (color, status) VALUES (?, 'PENDING')", (color,))
        conn.commit()
    return jsonify({"status": "success", "message": f"{color} added to queue."})

# -------- API: GET PENDING ORDERS --------
@app.route('/api/get_queue', methods=['GET'])
def get_queue():
    with sqlite3.connect(DB_FILE, timeout=5) as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT id, color FROM orders WHERE status = 'PENDING'")
        rows = cursor.fetchall()
        
    queue = [{"id": row[0], "color": row[1]} for row in rows]
    return jsonify(queue)

# -------- API: COMPLETE ORDER --------
@app.route('/api/complete_order', methods=['POST'])
def complete_order():
    data = request.json
    order_id = data.get('id')
    
    with sqlite3.connect(DB_FILE, timeout=5) as conn:
        cursor = conn.cursor()
        cursor.execute("UPDATE orders SET status = 'COMPLETED' WHERE id = ?", (order_id,))
        conn.commit()
    return jsonify({"status": "success"})


# -------- API: ADD INVENTORY (Optimized UPSERT) --------
@app.route('/api/add_inventory', methods=['POST'])
def add_inventory():
    data = request.json
    color = data.get('color')

    if not color:
        return jsonify({"status": "error", "message": "No color provided"}), 400

    with sqlite3.connect(DB_FILE, timeout=5) as conn:
        cursor = conn.cursor()
        # Single atomic operation to insert or update safely
        cursor.execute("""
            INSERT INTO inventory (color, count) 
            VALUES (?, 1) 
            ON CONFLICT(color) DO UPDATE SET count = count + 1
        """, (color,))
        conn.commit()

    return jsonify({"status": "success", "message": f"{color} added to inventory"})

# ------ API: VIEW INVENTORY --------
@app.route('/api/get_inventory', methods=['GET'])
def get_inventory():
    with sqlite3.connect(DB_FILE, timeout=5) as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT color, count FROM inventory")
        rows = cursor.fetchall()

    inventory = [{"color": row[0], "count": row[1]} for row in rows]
    return jsonify(inventory)


if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000, debug=True, threaded=True)