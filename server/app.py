from flask import Flask, request, jsonify, render_template_string

app = Flask(__name__)

# Store the latest data
latest_data = {}

@app.route('/message', methods=['POST'])
def receive_message():
    global latest_data
    latest_data = request.get_json()
    print(f"Received: {latest_data}")
    return jsonify({"status": "ok"}), 200

@app.route('/data', methods=['GET'])
def get_data():
    return jsonify(latest_data)

@app.route('/')
def index():
    return render_template_string(HTML)

HTML = """
<!DOCTYPE html>
<html>
<head>
    <title>Pico Dashboard</title>
    <style>
        body { font-family: Arial, sans-serif; padding: 40px; }
        .card { background: #f0f0f0; padding: 20px; border-radius: 8px; width: 300px; }
        .value { font-size: 2em; font-weight: bold; color: #333; }
        .label { color: #888; margin-top: 5px; }
    </style>
</head>
<body>
    <h1>Grass Height Monitor</h1>
    <div class="card">
        <div class="value" id="tof">--</div>
        <div class="label">ToF (mm)</div>
    </div>
    <br>
    <div class="card">
        <div class="value" id="sonic">--</div>
        <div class="label">Sonic (mm)</div>
    </div>

    <script>
        // Fetch new data every 2 seconds
        setInterval(() => {
            fetch('/data')
                .then(res => res.json())
                .then(data => {
                    document.getElementById('tof').textContent = 
                        data.grassHeightTof ?? '--';
                    document.getElementById('sonic').textContent = 
                        data.grassHeightSonic ?? '--';
                });
        }, 2000);
    </script>
</body>
</html>
"""

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5002)