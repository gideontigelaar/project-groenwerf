from flask import Flask, request, jsonify
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
    user=credentials.DB_USER,
    password=credentials.DB_PASSWORD,
    database=credentials.DB_NAME
)

def get_db():
    return db_pool.get_connection()

@app.route('/', methods=['GET'])
def health_check():
    return jsonify({
        "service": "groenwerf-api",
        "status": "online",
        "timestamp": datetime.now(timezone.utc).isoformat()
    }), 200

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

if __name__ == '__main__':
    app.run(host=credentials.FLASK_HOST, port=credentials.FLASK_PORT, debug=True)