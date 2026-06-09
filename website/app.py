import math
import os
import threading
import time
import datetime

from flask import Flask, jsonify, render_template, request, Response, make_response

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
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

CACHE_TTL = 300
_cache = {"rows": [], "ts": 0.0, "source": "none", "fields": []}
_cache_lock = threading.Lock()
_bg_running = threading.Event()

_arc_layer = None
_arc_fields_layer = None
_arc_lock = threading.Lock()

def _arcgis_configured():
    return bool(credentials and GIS and getattr(credentials, "ARCGIS_LAYER_URL", None) and getattr(credentials, "ARCGIS_USERNAME", None))

def _get_layer():
    global _arc_layer
    if _arc_layer is not None: return _arc_layer
    with _arc_lock:
        if _arc_layer is None and _arcgis_configured():
            gis = GIS("https://www.arcgis.com", credentials.ARCGIS_USERNAME, credentials.ARCGIS_PASSWORD)
            _arc_layer = FeatureLayer(credentials.ARCGIS_LAYER_URL, gis=gis)
    return _arc_layer

def _get_fields_layer():
    global _arc_fields_layer
    if _arc_fields_layer is not None: return _arc_fields_layer
    with _arc_lock:
        if _arc_fields_layer is None and _arcgis_configured():
            gis = GIS("https://www.arcgis.com", credentials.ARCGIS_USERNAME, credentials.ARCGIS_PASSWORD)
            fields_url = getattr(credentials, "ARCGIS_FIELDS_URL", None)
            if fields_url: _arc_fields_layer = FeatureLayer(fields_url, gis=gis)
    return _arc_fields_layer

def _attr(attr, *names):
    for n in names:
        if n in attr and attr[n] is not None: return attr[n]
    lower = {k.lower(): v for k, v in attr.items()}
    for n in names:
        v = lower.get(n.lower())
        if v is not None: return v
    return None

def _to_int(v):
    if v is None or v == "" or v == "NULL": return None
    try: return int(float(v))
    except (ValueError, TypeError): return None

def _to_float(v):
    if v is None or v == "" or v == "NULL": return None
    try:
        f = float(v)
        return None if math.isnan(f) else f
    except (ValueError, TypeError): return None

def _norm_timestamp(v):
    if v is None: return None
    if isinstance(v, (int, float)):
        try: return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(v / 1000.0))
        except (ValueError, OSError): return None
    return str(v).replace("T", " ").replace("Z", "")

def _fetch_from_arcgis():
    layer = _get_layer()
    rows = []
    offset = 0
    limit = 2000

    # fetch in batches until nothing left
    while True:
        feature_set = layer.query(
            where="1=1",
            out_fields="ToF_Height_mm,Sonic_Height_mm,Measured_At",
            return_geometry=True,
            result_record_count=limit,
            result_offset=offset,
            out_sr=4326,
        )
        if not feature_set.features: break

        for f in feature_set.features:
            attr = f.attributes or {}
            geom = f.geometry or {}
            rows.append({
                "tof_mm": _to_int(_attr(attr, "ToF_Height_mm", "tof_mm")),
                "sonic_mm": _to_int(_attr(attr, "Sonic_Height_mm", "sonic_mm")),
                "measured_at": _norm_timestamp(_attr(attr, "Measured_At", "measured_at")),
                "longitude": _to_float(geom.get("x")),
                "latitude": _to_float(geom.get("y")),
            })

        if len(feature_set.features) < limit: break
        offset += limit

    rows.sort(key=lambda r: str(r.get("measured_at") or ""), reverse=True)
    return rows

def _fetch_rows():
    if _arcgis_configured():
        try:
            rows = _fetch_from_arcgis()
            if rows: return rows, "arcgis"
        except Exception as exc: app.logger.error("arcgis fetch failed: %s", exc)
    return [], "none"

def _do_refresh():
    rows, source = _fetch_rows()
    fields = []
    try:
        flayer = _get_fields_layer()
        if flayer:
            fset = flayer.query(where="1=1", out_fields="*", return_geometry=True, out_sr=4326)
            fields = [{"attributes": f.attributes, "geometry": f.geometry} for f in fset.features]
    except Exception: pass

    with _cache_lock:
        if rows:
            _cache["rows"] = rows
            _cache["source"] = source
        if fields:
            _cache["fields"] = fields
        _cache["ts"] = time.time()

def _trigger_background_refresh():
    if _bg_running.is_set(): return
    _bg_running.set()
    def _run():
        try: _do_refresh()
        finally: _bg_running.clear()
    threading.Thread(target=_run, daemon=True).start()

def get_rows():
    now = time.time()
    with _cache_lock:
        fresh = (now - _cache["ts"]) < CACHE_TTL
        have = bool(_cache["rows"])

    # fallback to sync fetch if empty, else background update
    if not (fresh and have):
        if not have: _do_refresh()
        else: _trigger_background_refresh()

    with _cache_lock:
        return _cache.get("rows", []), _cache.get("source", "none"), _cache.get("fields", [])

def quality_grade(h):
    if h is None: return 4
    if h <= 70: return 0
    if h <= 80: return 1
    if h <= 90: return 2
    if h <= 100: return 3
    return 4

QUALITY_LABELS = ["A+", "A", "B", "C", "D"]
QUALITY_COLORS = ["#22c55e", "#84cc16", "#eab308", "#f97316", "#ef4444"]

def _in_poly(x, y, rings):
    if not x or not y or not rings: return False
    inside = False

    # raycast algorithm for point in polygon
    for ring in rings:
        for i in range(len(ring)):
            p1x, p1y = ring[i]
            p2x, p2y = ring[(i + 1) % len(ring)]
            if ((p1y > y) != (p2y > y)) and (x < (p2x - p1x) * (y - p1y) / (p2y - p1y + 1e-9) + p1x):
                inside = not inside
    return inside

def build_report(rows, fields, days=None, target_field_id=None):
    now = datetime.datetime.now()

    if days:
        try:
            cutoff = now - datetime.timedelta(days=int(days))
            cutoff_str = cutoff.strftime("%Y-%m-%d %H:%M:%S")
            rows = [r for r in rows if str(r.get("measured_at", "")) >= cutoff_str]
        except Exception: pass

    counts = [0, 0, 0, 0, 0]
    field_summaries = []

    for i, f in enumerate(fields):
        name = _attr(f.get("attributes", {}), "Name", "Naam", "Field_Name") or f"Veld {i+1}"
        rings = f.get("geometry", {}).get("rings", [])
        f_rows = [r for r in rows if _in_poly(r.get("longitude"), r.get("latitude"), rings)]

        if not f_rows: continue

        latest_day = f_rows[0].get("measured_at", "")[:10]
        day_rows = [r for r in f_rows if str(r.get("measured_at", "")).startswith(latest_day)]
        f_heights_day = [r.get("tof_mm") or r.get("sonic_mm") or 0 for r in day_rows]
        f_avg = round(sum(f_heights_day) / len(f_heights_day)) if f_heights_day else 0
        lvl = quality_grade(f_avg)

        history = []
        days_dict = {}
        for r in f_rows:
            d = str(r.get("measured_at", ""))[:10]
            if d not in days_dict: days_dict[d] = []
            days_dict[d].append(r)

        for d, d_rows in list(days_dict.items())[:30]:
            d_avg = round(sum([(r.get("tof_mm") or r.get("sonic_mm") or 0) for r in d_rows]) / len(d_rows))
            history.append({
                "date": d,
                "time": str(d_rows[0].get("measured_at", ""))[11:16],
                "h": d_avg,
                "lvl": quality_grade(d_avg),
                "count": len(d_rows)
            })

        # detect big height drops indicating mowing
        for j in range(len(history)):
            is_mow = False
            if j < len(history) - 1:
                older_h = history[j+1]["h"]
                if older_h - history[j]["h"] > 30:
                    is_mow = True
            history[j]["is_mow"] = is_mow
            history[j]["title"] = "Maaibeurt" if is_mow else "Meting"

        action = "Direct" if lvl == 4 else ("Inplannen" if lvl == 3 else "N.v.t.")

        field_summaries.append({
            "id": i,
            "name": name,
            "total": len(f_rows),
            "avg": f_avg,
            "latest": f_rows[0].get("measured_at", "—"),
            "label": QUALITY_LABELS[lvl],
            "level": lvl,
            "action": action,
            "bar_pct": min(100, round((f_avg / 150.0) * 100)),
            "history": history
        })

    field_summaries.sort(key=lambda s: s["level"], reverse=True)

    if target_field_id is not None and target_field_id != "":
        field_summaries = [s for s in field_summaries if str(s["id"]) == str(target_field_id)]

    for s in field_summaries: counts[s["level"]] += s["total"]
    total = sum(counts)

    # calculate svg values for donut chart
    r_circle = 60
    circ = 2 * math.pi * r_circle
    segments = []
    offset = 0.0
    for lvl, color in enumerate(QUALITY_COLORS):
        frac = (counts[lvl] / total) if total else 0
        seg_len = frac * circ
        segments.append({"color": color, "dash": f"{seg_len:.2f} {circ - seg_len:.2f}", "offset": f"{-offset:.2f}"})
        offset += seg_len

    latest_formatted = "—"
    if rows:
        raw_ts = str(rows[0].get("measured_at", "—"))
        if len(raw_ts) >= 16:
            dt = datetime.datetime.strptime(raw_ts[:16], "%Y-%m-%d %H:%M")
            latest_formatted = dt.strftime("%d-%m %H:%M")

    return {
        "total": total,
        "avg": round(sum([s["avg"] * s["total"] for s in field_summaries]) / total) if total else 0,
        "counts": counts,
        "pcts": [round((c / total) * 100) if total else 0 for c in counts],
        "latest": latest_formatted,
        "generated": time.strftime("%d-%m-%Y %H:%M"),
        "fields": field_summaries,
        "donut": {"circ": round(circ, 2), "segments": segments},
        "target_field": target_field_id is not None and target_field_id != ""
    }

@app.route("/api/fields")
def api_fields():
    _, _, fields = get_rows()
    return jsonify({"features": fields})

@app.route("/api/data")
def api_data():
    rows, source, _ = get_rows()
    return jsonify({"data": rows, "meta": {"source": source, "total": len(rows)}})

@app.route("/api/summary")
def api_summary():
    days = request.args.get("days", "30")
    if days == "": days = None
    field_id = request.args.get("field_id")
    rows, source, fields = get_rows()
    rep = build_report(rows, fields, days=days, target_field_id=field_id)
    rep["source"] = source
    return jsonify(rep)

@app.route("/api/sync", methods=["POST"])
def api_sync():
    _do_refresh()
    return jsonify({"status": "ok"})

@app.route("/")
def dashboard(): return render_template("dashboard.html", active_page="dashboard")

@app.route("/fields")
def fields(): return render_template("fields.html", active_page="fields")

@app.route("/report")
def report(): return render_template("report.html", active_page="report")

@app.route("/download-pdf")
def download_pdf():
    from weasyprint import HTML
    field_id = request.args.get("field")
    days = request.args.get("days")
    if days == "": days = None
    rows, _, fields = get_rows()

    html = render_template("pdf.html", r=build_report(rows, fields, days=days, target_field_id=field_id))

    resp = make_response(HTML(string=html, base_url=request.url_root).write_pdf())
    resp.headers["Content-Type"] = "application/pdf"
    resp.headers["Content-Disposition"] = "attachment; filename=veldbeheer-rapport.pdf"
    return resp

if __name__ == "__main__":
    app.run(port=3000, debug=True)