from flask import Flask, jsonify, render_template, request, Response, make_response
import urllib.request
import urllib.parse
import json
import os
import time
from weasyprint import HTML

try:
    import credentials
except ImportError:
    credentials = None

try:
    from arcgis.gis import GIS
    from arcgis.features import FeatureLayer
except ImportError:
    GIS = None
    FeatureLayer = None

app = Flask(__name__)

# --- cache setup ---
CACHE_TTL = 300 # 5 minutes
_data_cache = []
_fields_cache = []
_last_arcgis_fetch = 0.0

def refresh_arcgis_cache():
    global _data_cache, _fields_cache, _last_arcgis_fetch

    now = time.time()
    # return early if cache is still fresh
    if (now - _last_arcgis_fetch) < CACHE_TTL and _data_cache:
        return

    if not credentials or not GIS:
        print("ERROR: missing credentials.py or arcgis library")
        return

    try:
        print("Fetching data from ArcGIS...")
        gis = GIS("https://www.arcgis.com", credentials.ARCGIS_USERNAME, credentials.ARCGIS_PASSWORD)
        layer = FeatureLayer(credentials.ARCGIS_LAYER_URL)

        # We removed 'order_by_fields' to prevent ArcGIS from throwing a 400 Bad Request error.
        # We will sort it in Python instead.
        feature_set = layer.query(
            where="1=1",
            out_fields="*",
            return_geometry=True,
            result_record_count=500
        )

        rows = []
        fields = set()

        for f in feature_set.features:
            attr = f.attributes
            geom = f.geometry

            # Use 'Field_Name' if it exists, otherwise default to 'Onbekend'
            field_name = attr.get("Field_Name", "Onbekend")

            rows.append({
                "tof_mm": attr.get("ToF_Height_mm"),
                "sonic_mm": attr.get("Sonic_Height_mm"),
                "measured_at": attr.get("Measured_at"),
                "longitude": geom.get("x") if geom else None,
                "latitude": geom.get("y") if geom else None,
                "field_name": field_name
            })

            if field_name and field_name != "Onbekend":
                fields.add(field_name)

        # Sort the data in Python by measured_at (Measured_at) descending
        rows.sort(key=lambda r: str(r.get("measured_at") or ""), reverse=True)

        _data_cache = rows
        _fields_cache = [{"field_name": name} for name in sorted(list(fields))]
        _last_arcgis_fetch = now

        print(f"SUCCESS: Fetched and cached {len(_data_cache)} records from ArcGIS.")

    except Exception as e:
        print(f"ARCGIS FETCH ERROR: {e}")


# --- API: data ---
@app.route('/api/data')
def api_data():
    refresh_arcgis_cache()

    sort = request.args.get('sort', 'measured_at')

    # default sorting is already descending timestamp from arcgis
    if sort == 'measured_at':
        return jsonify(_data_cache[:100])

    allowed = {'tof_mm', 'sonic_mm', 'longitude', 'latitude'}
    if sort not in allowed:
        return jsonify(_data_cache[:100])

    def sort_key(r):
        v = r.get(sort)
        if v is None:
            return -999999
        try:
            return float(v)
        except ValueError:
            return str(v)

    sorted_rows = sorted(_data_cache, key=sort_key, reverse=True)
    return jsonify(sorted_rows[:100])


# --- API: fields ---
@app.route('/api/fields')
def api_fields():
    refresh_arcgis_cache()
    return jsonify(_fields_cache)


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

@app.route('/download-pdf')
def download_pdf():
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
    pdf_bytes = HTML(string=html).write_pdf()

    response = make_response(pdf_bytes)
    response.headers['Content-Type'] = 'application/pdf'
    response.headers['Content-Disposition'] = 'attachment; filename=veldbeheer-rapport.pdf'

    return response


if __name__ == '__main__':
    app.run(port=3000, debug=True)