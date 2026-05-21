from flask import Flask, request, jsonify, render_template_string
import mysql.connector

app = Flask(__name__)

# MySQL connection
db = mysql.connector.connect(
    host="host",
    user="username",
    password="password",
    database="database"
)

cursor = db.cursor(dictionary=True)

@app.route('/data', methods=['POST'])
def receive_data():

    data = request.get_json()

    print("Received:", data)

    for item in data:

        sql = """
        INSERT INTO sensor_readings
        (
            latitude,
            longitude,
            tof_mm,
            sonic_mm,
            temperature,
            measured_at
        )
        VALUES (%s, %s, %s, %s, %s, NOW())
        """

        values = (
            item.get("lat"),
            item.get("lon"),
            item.get("grassHeightTof"),
            item.get("grassHeightSonicMedian"),
            None
        )

        cursor.execute(sql, values)

    db.commit()

    return jsonify({"status": "ok"}), 200

@app.route('/data', methods=['GET'])
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

    const res = await fetch('/data');
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
    app.run(host='0.0.0.0', port=5002)