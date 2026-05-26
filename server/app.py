from flask import Flask, request, jsonify, render_template_string
import mysql.connector
from mysql.connector import pooling, Error
from datetime import datetime, timezone
import sys

try:
    import credentials
except ModuleNotFoundError:
    sys.exit(
        "ERROR: credentials.py not found.\n"
        "Copy server/credentials.py.template to server/credentials.py and fill in your values."
    )

app = Flask(__name__)

db_pool = pooling.MySQLConnectionPool(
    pool_name="sensor_pool",
    pool_size=5,
    host=credentials.DB_HOST,
    user=credentials.DB_USER,
    password=credentials.DB_PASSWORD,
    database=credentials.DB_NAME
)

def get_db_connection():
    return db_pool.get_connection()

@app.route('/sensor-data', methods=['POST'])
def receive_data():

    # API Key Authentication
    if request.headers.get('X-API-Key') != credentials.API_KEY:
        return jsonify({"error": "unauthorized"}), 401

    # Validate JSON
    data = request.get_json()

    if not data:
        return jsonify({"error": "invalid json"}), 400

    if not isinstance(data, list):
        return jsonify({"error": "expected a list"}), 400

    db = None
    cursor = None

    try:
        db = get_db_connection()
        cursor = db.cursor(dictionary=True)

        for item in data:

            if not isinstance(item, dict):
                continue

            measured_at = item.get("measured_at")

            if measured_at is not None:

                try:

                    # Parse ISO 8601 UTC timestamp
                    measured_at = datetime.strptime(
                        measured_at,
                        "%Y-%m-%dT%H:%M:%SZ"
                    ).strftime("%Y-%m-%d %H:%M:%S")

                except (ValueError, TypeError):

                    print("Invalid timestamp:", measured_at)

                    measured_at = None

            tof_val          = item.get("grassHeightTof")
            sonic_median_val = item.get("grassHeightSonicMedian")
            sonic_accel_val  = item.get("grassHeightSonicAccel")

            raw_sonic  = item.get("sonic_raw_mm")
            raw_tof    = item.get("tof_raw_mm")
            raw_accel_x = item.get("accel_raw_x")
            raw_accel_y = item.get("accel_raw_y")
            raw_accel_z = item.get("accel_raw_z")

            processed_id = None
            raw_id       = None

            if any([tof_val, sonic_median_val, sonic_accel_val]):
                sql_processed = """
                INSERT INTO sensor_readings
                (
                    latitude,
                    longitude,
                    tof_mm,
                    sonic_median_mm,
                    sonic_accel_mm,
                    temperature,
                    measured_at
                )
                VALUES (%s, %s, %s, %s, %s, %s, %s)
                """
                values_processed = (
                    item.get("lat"),
                    item.get("lon"),
                    tof_val,
                    sonic_median_val,
                    sonic_accel_val,
                    item.get("temperature"),
                    measured_at
                )
                cursor.execute(sql_processed, values_processed)
                processed_id = cursor.lastrowid

            if any([raw_sonic, raw_tof, raw_accel_x, raw_accel_y, raw_accel_z]):
                sql_raw = """
                INSERT INTO raw_sensor_readings
                (
                    sonic_raw_mm,
                    tof_raw_mm,
                    accel_raw_x,
                    accel_raw_y,
                    accel_raw_z,
                    measured_at
                )
                VALUES (%s, %s, %s, %s, %s, %s)
                """
                values_raw = (
                    raw_sonic,
                    raw_tof,
                    raw_accel_x,
                    raw_accel_y,
                    raw_accel_z,
                    measured_at
                )
                cursor.execute(sql_raw, values_raw)
                raw_id = cursor.lastrowid

            if raw_id is not None and processed_id is not None:
                cursor.execute(
                    "INSERT INTO reading_pairs (raw_id, processed_id) VALUES (%s, %s)",
                    (raw_id, processed_id)
                )

        db.commit()

        return jsonify({"status": "ok"}), 200

    except Error as e:

        if db:
            db.rollback()

        return jsonify({
            "error": "database error",
            "details": str(e)
        }), 500

    except Exception as e:

        if db:
            db.rollback()

        return jsonify({
            "error": "server error",
            "details": str(e)
        }), 500

    finally:

        if cursor:
            cursor.close()

        if db and db.is_connected():
            db.close()


@app.route('/sensor-data', methods=['GET'])
def get_data():

    db = None
    cursor = None

    try:
        db = get_db_connection()
        cursor = db.cursor(dictionary=True)

        cursor.execute("""
            SELECT *
            FROM sensor_readings
            ORDER BY id DESC
            LIMIT 1
        """)

        latest = cursor.fetchone()

        if latest is None:
            return jsonify({})

        return jsonify(latest)

    except Error as e:

        return jsonify({
            "error": "database error",
            "details": str(e)
        }), 500

    finally:

        if cursor:
            cursor.close()

        if db and db.is_connected():
            db.close()


@app.route('/')
def index():
    return render_template_string(HTML)

HTML = """
<!DOCTYPE html>
<html>
<head>
    <title>Pico Dashboard</title>

    <style>
        body {
            font-family: Arial, sans-serif;
            padding: 40px;
            background: #fafafa;
        }

        h1 {
            margin-bottom: 30px;
        }

        .card {
            background: white;
            padding: 20px;
            margin-bottom: 20px;
            border-radius: 12px;
            width: 320px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.08);
        }

        .label {
            color: #666;
            margin-top: 10px;
        }

        .value {
            font-size: 2em;
            font-weight: bold;
        }

        .timestamp {
            color: #888;
            font-size: 0.9em;
            margin-top: 15px;
        }
    </style>
</head>

<body>

<h1>Grass Height Monitor</h1>

<div class="card">
    <div class="value" id="tof">--</div>
    <div class="label">ToF (mm)</div>
</div>

<div class="card">
    <div class="value" id="sonic">--</div>
    <div class="label">Sonic Median / Accel (mm)</div>
</div>

<div class="card">
    <div class="value" id="latlon">--</div>
    <div class="label">Location</div>
</div>

<div class="card">
    <div class="value" id="timestamp">--</div>
    <div class="label">Measured At</div>
</div>

<script>

async function updateData() {

    try {

        const res = await fetch('/sensor-data');

        if (!res.ok) {
            console.error('Failed to fetch data');
            return;
        }

        const data = await res.json();

        if (!data || Object.keys(data).length === 0) {
            return;
        }

        document.getElementById('tof').textContent =
            data.tof_mm ?? '--';

        document.getElementById('sonic').textContent =
            (data.sonic_median_mm ?? '--') + ' / ' + (data.sonic_accel_mm ?? '--');

        document.getElementById('latlon').textContent =
            `${data.latitude ?? '--'}, ${data.longitude ?? '--'}`;

        document.getElementById('timestamp').textContent =
            data.measured_at ?? '--';

    } catch (err) {

        console.error('Error updating data:', err);

    }
}

updateData();

setInterval(updateData, 2000);

</script>

</body>
</html>
"""

if __name__ == '__main__':

    app.run(
        host=credentials.FLASK_HOST,
        port=credentials.FLASK_PORT,
        debug=True
    )