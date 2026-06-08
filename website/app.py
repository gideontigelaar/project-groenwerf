import math
import os
import threading
import time

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

# status thresholds
THRESHOLD_MOW = 400
THRESHOLD_WATCH = 80

# cache state
CACHE_TTL = 300
_cache = {"rows": [], "ts": 0.0, "source": "none"}
_cache_lock = threading.Lock()
_bg_running = threading.Event()

# arcgis connection
_arc_layer = None
_arc_lock = threading.Lock()

def _arcgis_configured():
    return bool(
        credentials
        and GIS
        and getattr(credentials, "ARCGIS_LAYER_URL", None)
        and getattr(credentials, "ARCGIS_USERNAME", None)
    )

_arc_fields_layer = None

def _get_layer():
    # lazily create and cache the featurelayer
    global _arc_layer
    if _arc_layer is not None:
        return _arc_layer
    with _arc_lock:
        if _arc_layer is None:
            gis = GIS(
                "https://www.arcgis.com",
                credentials.ARCGIS_USERNAME,
                credentials.ARCGIS_PASSWORD,
            )
            _arc_layer = FeatureLayer(credentials.ARCGIS_LAYER_URL, gis=gis)
    return _arc_layer

def _get_fields_layer():
    global _arc_fields_layer
    if _arc_fields_layer is not None:
        return _arc_fields_layer
    with _arc_lock:
        if _arc_fields_layer is None:
            gis = GIS(
                "https://www.arcgis.com",
                credentials.ARCGIS_USERNAME,
                credentials.ARCGIS_PASSWORD,
            )
            fields_url = getattr(credentials, "ARCGIS_FIELDS_URL", None)
            if fields_url:
                _arc_fields_layer = FeatureLayer(fields_url, gis=gis)
    return _arc_fields_layer

def _attr(attr, *names):
    for n in names:
        if n in attr and attr[n] is not None:
            return attr[n]
    lower = {k.lower(): v for k, v in attr.items()}
    for n in names:
        v = lower.get(n.lower())
        if v is not None:
            return v
    return None

def _to_int(v):
    if v is None or v == "" or v == "NULL":
        return None
    try:
        return int(float(v))
    except (ValueError, TypeError):
        return None

def _to_float(v):
    if v is None or v == "" or v == "NULL":
        return None
    try:
        f = float(v)
        return None if math.isnan(f) else f
    except (ValueError, TypeError):
        return None

def _norm_timestamp(v):
    if v is None:
        return None
    if isinstance(v, (int, float)):
        try:
            return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(v / 1000.0))
        except (ValueError, OSError):
            return None
    return str(v).replace("T", " ").replace("Z", "")

def _fetch_from_arcgis():
    layer = _get_layer()
    feature_set = layer.query(
        where="1=1",
        out_fields="ToF_Height_mm,Sonic_Height_mm,Measured_At",
        return_geometry=True,
        result_record_count=500,
        out_sr=4326,
    )
    rows = []
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
    rows.sort(key=lambda r: str(r.get("measured_at") or ""), reverse=True)
    return rows

def _fetch_rows():
    if _arcgis_configured():
        try:
            rows = _fetch_from_arcgis()
            if rows:
                return rows, "arcgis"
        except Exception as exc:
            app.logger.error("arcgis fetch failed: %s", exc)
    return [], "none"

def _do_refresh():
    rows, source = _fetch_rows()
    with _cache_lock:
        if rows:
            _cache["rows"] = rows
            _cache["source"] = source
        _cache["ts"] = time.time()

def _trigger_background_refresh():
    if _bg_running.is_set():
        return
    _bg_running.set()

    def _run():
        try:
            _do_refresh()
        finally:
            _bg_running.clear()

    threading.Thread(target=_run, daemon=True).start()

def get_rows():
    # serves cached data while refreshing
    now = time.time()
    with _cache_lock:
        fresh = (now - _cache["ts"]) < CACHE_TTL
        have = bool(_cache["rows"])
        rows, source = _cache["rows"], _cache["source"]

    if fresh and have:
        return rows, source
    if not have:
        _do_refresh()
    else:
        _trigger_background_refresh()
    with _cache_lock:
        return _cache["rows"], _cache["source"]

def status_level(row):
    h = (row.get("tof_mm") or row.get("sonic_mm") or 0)
    if h >= THRESHOLD_MOW:
        return 2
    if h >= THRESHOLD_WATCH:
        return 1
    return 0

STATUS_LABELS = ["Goed", "Let op", "Maaien"]

def build_report(rows):
    # compute report parameters for pdf
    counts = [0, 0, 0]
    heights = []
    for r in rows:
        counts[status_level(r)] += 1
        heights.append(r.get("tof_mm") or r.get("sonic_mm") or 0)

    total = len(rows)
    avg = round(sum(heights) / len(heights)) if heights else 0
    latest = rows[0]["measured_at"] if rows else None

    recent = []
    for r in rows[:18]:
        lvl = status_level(r)
        h = r.get("tof_mm") or r.get("sonic_mm") or 0
        recent.append({
            "tof_mm": r.get("tof_mm"),
            "sonic_mm": r.get("sonic_mm"),
            "height": h,
            "measured_at": r.get("measured_at") or "—",
            "time": (r.get("measured_at") or "—")[11:16] or "—",
            "date": (r.get("measured_at") or "—")[:10] or "—",
            "label": STATUS_LABELS[lvl],
            "level": lvl,
            "bar_pct": min(100, round(h / 8)),
        })

    r_circle = 60
    circ = 2 * math.pi * r_circle
    segments = []
    offset = 0.0
    for lvl, color in enumerate(["#3b6d11", "#f59e0b", "#ef4444"]):
        frac = (counts[lvl] / total) if total else 0
        seg_len = frac * circ
        segments.append({
            "color": color,
            "dash": f"{seg_len:.2f} {circ - seg_len:.2f}",
            "offset": f"{-offset:.2f}",
        })
        offset += seg_len

    return {
        "total": total,
        "avg": avg,
        "counts_ok": counts[0],
        "counts_warn": counts[1],
        "counts_mow": counts[2],
        "pct_ok": round((counts[0] / total) * 100) if total else 0,
        "pct_warn": round((counts[1] / total) * 100) if total else 0,
        "pct_mow": round((counts[2] / total) * 100) if total else 0,
        "latest": latest or "—",
        "generated": time.strftime("%d-%m-%Y %H:%M"),
        "recent": recent,
        "donut": {"circ": round(circ, 2), "segments": segments},
        "thresholds": {"mow": THRESHOLD_MOW, "watch": THRESHOLD_WATCH},
    }

@app.route("/api/fields")
def api_fields():
    layer = _get_fields_layer()
    if not layer:
        return jsonify({"features": []})
    try:
        fset = layer.query(where="1=1", out_fields="*", return_geometry=True, out_sr=4326)
        features = [{"attributes": f.attributes, "geometry": f.geometry} for f in fset.features]
        return jsonify({"features": features})
    except Exception as exc:
        app.logger.error("arcgis fields fetch failed: %s", exc)
        return jsonify({"error": str(exc)}), 500

@app.route("/api/data")
def api_data():
    rows, source = get_rows()
    sort = request.args.get("sort", "measured_at")
    allowed = {"tof_mm", "sonic_mm", "longitude", "latitude"}
    if sort in allowed:
        def key(r):
            v = r.get(sort)
            return v if isinstance(v, (int, float)) else -1
        rows = sorted(rows, key=key, reverse=True)

    return jsonify({"data": rows[:100], "meta": {"source": source, "total": len(rows)}})

@app.route("/api/sync", methods=["POST"])
def api_sync():
    # manual refresh forced from dashboard
    _do_refresh()
    return jsonify({"status": "ok"})

@app.route("/")
def dashboard():
    return render_template("dashboard.html", active_page="dashboard")

@app.route("/fields")
def fields():
    data_url = getattr(credentials, "ARCGIS_LAYER_URL", "") if credentials else ""
    fields_url = getattr(credentials, "ARCGIS_FIELDS_URL", "") if credentials else ""
    return render_template("fields.html", active_page="fields", data_url=data_url, fields_url=fields_url)

@app.route("/report")
def report():
    return render_template("report.html", active_page="report")

def _render_pdf():
    from weasyprint import HTML
    rows, _ = get_rows()
    html = render_template("pdf.html", r=build_report(rows))
    return HTML(string=html, base_url=request.url_root).write_pdf()

@app.route("/pdf")
def pdf_inline():
    return Response(
        _render_pdf(),
        mimetype="application/pdf",
        headers={"Content-Disposition": "inline; filename=veldbeheer-rapport.pdf"},
    )

@app.route("/download-pdf")
def download_pdf():
    resp = make_response(_render_pdf())
    resp.headers["Content-Type"] = "application/pdf"
    resp.headers["Content-Disposition"] = "attachment; filename=veldbeheer-rapport.pdf"
    return resp

if __name__ == "__main__":
    app.run(port=3000, debug=True)