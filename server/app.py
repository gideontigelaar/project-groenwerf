from flask import Flask, request, jsonify, render_template_string
import mysql.connector
from mysql.connector import pooling, Error
from datetime import datetime, timezone
import sys

try:
    import credentials
except ModuleNotFoundError:
    sys.exit("ERROR: credentials.py not found.")

app = Flask(__name__)

db_pool = pooling.MySQLConnectionPool(
    pool_name="sensor_pool",
    pool_size=5,
    host=credentials.DB_HOST,
    port=credentials.DB_PORT,
    user=credentials.DB_USER,
    password=credentials.DB_PASSWORD,
    database=credentials.DB_NAME
)

def get_db():
    return db_pool.get_connection()

# POST /sensor-data
@app.route('/sensor-data', methods=['POST'])
def receive_data():
    if request.headers.get('X-API-Key') != credentials.API_KEY:
        return jsonify({"error": "unauthorized"}), 401

    data = request.get_json()
    if not data or not isinstance(data, list):
        return jsonify({"error": "expected a JSON array"}), 400

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor()

        for item in data:
            if not isinstance(item, dict):
                continue

            measured_at = item.get("measured_at")
            if measured_at:
                try:
                    measured_at = datetime.strptime(measured_at, "%Y-%m-%dT%H:%M:%SZ") \
                                          .strftime("%Y-%m-%d %H:%M:%S")
                except (ValueError, TypeError):
                    measured_at = None

            if measured_at is None:
                measured_at = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")

            cursor.execute("""
                INSERT INTO sensor_readings
                    (latitude, longitude, tof_mm, sonic_mm, temperature, sonic_raw_mm, tof_raw_mm, accel_raw_x, accel_raw_y, accel_raw_z, measured_at)
                VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            """, (
                item.get("lat"),
                item.get("lon"),
                item.get("grassHeightTof"),
                item.get("grassHeightSonic"),
                item.get("temperature"),
                item.get("sonic_raw_mm"),
                item.get("tof_raw_mm"),
                item.get("accel_raw_x"),
                item.get("accel_raw_y"),
                item.get("accel_raw_z"),
                measured_at,
            ))

        db.commit()
        return jsonify({"status": "ok"}), 200

    except Error as e:
        if db: db.rollback()
        return jsonify({"error": "db error", "details": str(e)}), 500
    finally:
        if cursor: cursor.close()
        if db and db.is_connected(): db.close()

# GET /sensor-data
@app.route('/sensor-data', methods=['GET'])
def get_data():
    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor(dictionary=True)
        cursor.execute("SELECT * FROM sensor_readings ORDER BY id DESC LIMIT 1")
        row = cursor.fetchone()
        return jsonify(row or {})
    except Error as e:
        return jsonify({"error": "db error", "details": str(e)}), 500
    finally:
        if cursor: cursor.close()
        if db and db.is_connected(): db.close()

# dashboard
@app.route('/')
def index():
    return render_template_string(HTML)

HTML = """
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>groenwerf</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: monospace; background: #0e0e0e; color: #e0e0e0; padding: 32px; }
        h1 { font-size: 13px; color: #555; letter-spacing: 2px; text-transform: uppercase; margin-bottom: 24px; }
        table { border-collapse: collapse; width: 100%; max-width: 520px; }
        td { padding: 8px 0; border-bottom: 1px solid #1e1e1e; font-size: 14px; }
        td:first-child { color: #555; width: 180px; }
        td:last-child { color: #e0e0e0; }
        #status { font-size: 11px; color: #333; margin-top: 20px; }
    </style>
</head>
<body>
    <h1>grass monitor · live</h1>
    <table>
        <tr><td>tof</td><td><span id="tof">--</span> mm</td></tr>
        <tr><td>sonic</td><td><span id="sonic">--</span> mm</td></tr>
        <tr><td>temperature</td><td><span id="temp">--</span> °C</td></tr>
        <tr><td>location</td><td><span id="loc">--</span></td></tr>
        <tr><td>measured at</td><td><span id="ts">--</span></td></tr>
    </table>
    <p id="status">connecting...</p>
    <script>
        async function poll() {
            try {
                const r = await fetch('/sensor-data');
                const d = await r.json();
                if (!d || !d.id) return;
                document.getElementById('tof').textContent   = d.tof_mm   ?? '--';
                document.getElementById('sonic').textContent = d.sonic_mm ?? '--';
                document.getElementById('temp').textContent  = d.temperature ?? '--';
                document.getElementById('loc').textContent   =
                    (d.latitude && d.longitude) ? d.latitude + ', ' + d.longitude : '--';
                document.getElementById('ts').textContent    = d.measured_at ?? '--';
                document.getElementById('status').textContent = 'last update: ' + new Date().toLocaleTimeString();
            } catch(e) {
                document.getElementById('status').textContent = 'error: ' + e.message;
            }
        }
        poll();
        setInterval(poll, 2000);
    </script>
</body>
</html>
"""

if __name__ == '__main__':
    app.run(host=credentials.FLASK_HOST, port=credentials.FLASK_PORT, debug=True)