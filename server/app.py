from flask import Flask, request, jsonify, render_template_string
import mysql.connector
import sys

try:
    import credentials
except ModuleNotFoundError:
    sys.exit(
        "ERROR: credentials.py not found.\n"
        "Copy server/credentials.py.template to server/credentials.py and fill in your values."
    )

app = Flask(__name__)

db = mysql.connector.connect(
    host=credentials.DB_HOST,
    user=credentials.DB_USER,
    password=credentials.DB_PASSWORD,
    database=credentials.DB_NAME
)

cursor = db.cursor(dictionary=True)

@app.route('/sensor-data', methods=['POST'])
def receive_data():

    if request.headers.get('X-API-Key') != credentials.API_KEY:
        return jsonify({"error": "unauthorized"}), 401

    data = request.get_json()

    for item in data:
        sql_processed = """
        INSERT INTO sensor_readings
        (
            latitude,
            longitude,
            tof_mm,
            sonic_mm,
            temperature,
            measured_at
        )
        VALUES (%s, %s, %s, %s, %s, %s)
        """
        values_processed = (
            item.get("lat"),
            item.get("lon"),
            item.get("grassHeightTof"),
            item.get("grassHeightSonicMedian"),
            None,
            item.get("measured_at")
        )
        cursor.execute(sql_processed, values_processed)

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
            item.get("sonic_raw_mm"),
            item.get("tof_raw_mm"),
            item.get("accel_raw_x"),
            item.get("accel_raw_y"),
            item.get("accel_raw_z"),
            item.get("measured_at")
        )
        cursor.execute(sql_raw, values_raw)

    db.commit()

    return jsonify({"status": "ok"}), 200

@app.route('/sensor-data', methods=['GET'])
def get_data():
    cursor.execute("""
        SELECT *
        FROM sensor_readings
        ORDER BY id DESC
        LIMIT 1
    """)
    latest = cursor.fetchone()
    return jsonify(latest)


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
            font-family: Arial;
            padding: 40px;
        }

        .card {
            background: #f0f0f0;
            padding: 20px;
            margin-bottom: 20px;
            border-radius: 10px;
            width: 300px;
        }

        .value {
            font-size: 2em;
            font-weight: bold;
        }
    </style>
</head>
<body>

<h1>Grass Height Monitor</h1>

<div class="card">
    <div class="value" id="tof">--</div>
    <div>ToF (mm)</div>
</div>

<div class="card">
    <div class="value" id="sonic">--</div>
    <div>Sonic (mm)</div>
</div>

<script>
async function updateData() {

    const res = await fetch('/sensor-data');
    const data = await res.json();

    document.getElementById('tof').textContent =
        data.tof_mm ?? '--';

    document.getElementById('sonic').textContent =
        data.sonic_mm ?? '--';
}

setInterval(updateData, 2000);

updateData();
</script>

</body>
</html>
"""

if __name__ == '__main__':
    app.run(host=credentials.FLASK_HOST, port=credentials.FLASK_PORT)