from flask import Flask, jsonify, render_template, request
import mysql.connector
import urllib.request
import urllib.parse
import json
 
app = Flask(__name__)
 
DB_CONFIG = {
    'host': '167.99.39.255',
    'user': 'dashboard',
    'password': 'frikandel67',
    'database': 'groenwerf'
}
 
def get_db_connection():
    return mysql.connector.connect(**DB_CONFIG, connection_timeout=5)
 
 
# --- API: replaces data.php ---
@app.route('/api/data')
def api_data():
    try:
        conn = get_db_connection()
        cursor = conn.cursor(dictionary=True)
        cursor.execute(
            "SELECT tof_mm, sonic_median_mm, longitude, latitude, measured_at "
            "FROM sensor_readings ORDER BY measured_at DESC LIMIT 100"
        )
        rows = cursor.fetchall()
        cursor.close()
        conn.close()
        for row in rows:
            if row.get('measured_at'):
                row['measured_at'] = str(row['measured_at'])
        return jsonify(rows)
    except Exception:
        # DB offline or unreachable — return empty array so the frontend keeps working
        return jsonify([]), 200
 
 
# --- API: replaces geocode.php ---
@app.route('/api/geocode')
def api_geocode():
    lat = request.args.get('lat', '')
    lng = request.args.get('lng', '')
 
    if lat in ('', 'null') or lng in ('', 'null'):
        return jsonify({'address': 'Unknown'})
 
    url = (
        f"https://nominatim.openstreetmap.org/reverse"
        f"?lat={urllib.parse.quote(lat)}&lon={urllib.parse.quote(lng)}&format=json"
    )
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'GroenWerfApp/1.0'})
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read())
        address = data.get('display_name', 'Address not found')
        return jsonify({'address': address})
    except Exception:
        return jsonify({'address': 'Unknown'})
 
 
# --- Pages: replace index.php and rapport.php ---
@app.route('/')
@app.route('/index')
def index():
    return render_template('index.html', active_page="index")
 
@app.route('/rawdata')
def rawdata():
    return render_template('rawdata.html', active_page="rawdata")

@app.route('/rapport')
def rapport():
    return render_template('rapport.html', active_page="rapport")
 
 
if __name__ == '__main__':
    app.run(debug=True)