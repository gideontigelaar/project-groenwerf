from flask import Flask, jsonify, render_template, request, Response
import urllib.request
import urllib.parse
import json
import csv
import os
from weasyprint import HTML

app = Flask(__name__)

CSV_PATH = os.path.join(os.path.dirname(__file__), 'sensor_readings.csv')

def load_csv_data():
    rows = []
    with open(CSV_PATH, newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({
                'tof_mm':          None if row['tof_mm'] in ('', 'NULL') else row['tof_mm'],
                'sonic_median_mm': None if row['sonic_mm'] in ('', 'NULL') else row['sonic_mm'],
                'longitude':       None if row['longitude'] in ('', 'NULL') else row['longitude'],
                'latitude':        None if row['latitude'] in ('', 'NULL') else row['latitude'],
                'measured_at':     row['measured_at'],
            })
    return rows


# --- API: data ---
@app.route('/api/data')
def api_data():
    sort = request.args.get('sort', 'measured_at')
    allowed = {'measured_at', 'tof_mm', 'sonic_mm', 'longitude', 'latitude'}
    if sort not in allowed:
        sort = 'measured_at'

    rows = load_csv_data()

    def sort_key(r):
        v = r.get(sort) or r.get('sonic_median_mm') or r.get('measured_at')
        return v or ''

    rows.sort(key=sort_key, reverse=True)
    return jsonify(rows[:100])


# --- API: geocode ---
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


# --- Pages ---
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

@app.route("/pdf")
def generate_pdf():
    data = {
        "title": "Voorbeeld PDF",
        "description": "Dit is een PDF gegenereerd met Flask en WeasyPrint.",
        "items": [
            {"naam": "Item 1", "waarde": 100},
            {"naam": "Item 2", "waarde": 200},
            {"naam": "Item 3", "waarde": 300},
        ]
    }
 
    html = render_template("pdf.html", **data)
    pdf = HTML(string=html).write_pdf()
 
    return Response(
        pdf,
        mimetype="application/pdf",
        headers={"Content-Disposition": "inline; filename=output.pdf"}
    )
    # return html


if __name__ == '__main__':
    app.run(port=3000, debug=True)