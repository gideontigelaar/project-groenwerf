import math
import os
import threading
import time
import datetime
import requests
import urllib.parse

from flask import Flask, jsonify, render_template, request, make_response, redirect, url_for, session

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
app.secret_key = getattr(credentials, 'FLASK_SECRET_KEY', 'default-dev-key-change-me')
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

API_BASE_URL = getattr(credentials, 'API_BASE_URL', 'http://127.0.0.1:5002')
API_KEY = getattr(credentials, 'API_KEY', '')

CACHE_TTL = 300
_cache = {"rows": [], "ts": 0.0, "source": "none", "fields": []}
_cache_lock = threading.Lock()
_bg_running = threading.Event()

_arc_layer = None
_arc_fields_layer = None
_arc_lock = threading.Lock()

# authentication hooks
@app.before_request
def require_login():
    allowed_routes = ['login', 'register', 'static']
    if request.endpoint not in allowed_routes and 'user_id' not in session:
        if request.path.startswith('/api/'):
            return jsonify({"error": "unauthorized"}), 401
        return redirect(url_for('login'))

def _get_user_fields(user_id):
    try:
        resp = requests.get(
            f"{API_BASE_URL}/users/{user_id}/fields",
            headers={"X-API-Key": API_KEY},
            timeout=5
        )
        if resp.status_code == 200:
            return resp.json().get("fields", [])
    except Exception:
        pass
    return []

@app.route("/login", methods=["GET", "POST"])
def login():
    if 'user_id' in session:
        return redirect(url_for('dashboard'))

    error = None
    if request.method == "POST":
        username = request.form.get("username")
        password = request.form.get("password")
        try:
            resp = requests.post(
                f"{API_BASE_URL}/auth/login",
                json={"username": username, "password": password},
                headers={"X-API-Key": API_KEY},
                timeout=5
            )
            data = resp.json()
            if resp.status_code == 200 and data.get("status") == "ok":
                session['user_id'] = data['user']['id']
                session['name'] = data['user']['name']
                session['username'] = data['user']['username']
                session['role'] = data['user']['role']
                return redirect(url_for('dashboard'))
            else:
                error = data.get("error", "Ongeldige inloggegevens.")
        except Exception as e:
            error = "Kon niet verbinden met de server."

    return render_template("login.html", error=error)

@app.route("/register", methods=["GET", "POST"])
def register():
    if 'user_id' in session:
        return redirect(url_for('dashboard'))

    error = None
    success = False
    if request.method == "POST":
        name = request.form.get("name")
        username = request.form.get("username")
        password = request.form.get("password")
        confirm_password = request.form.get("confirm_password")
        invite_code = request.form.get("invite_code")

        if password != confirm_password:
            error = "Wachtwoorden komen niet overeen."
        else:
            try:
                resp = requests.post(
                    f"{API_BASE_URL}/auth/register",
                    json={"name": name, "username": username, "password": password, "invite_code": invite_code},
                    headers={"X-API-Key": API_KEY},
                    timeout=5
                )
                data = resp.json()
                if resp.status_code == 200 and data.get("status") == "ok":
                    success = True
                else:
                    error = data.get("error", "Registratie mislukt.")
            except Exception as e:
                error = "Kon niet verbinden met de server."

    return render_template("register.html", error=error, success=success)

@app.route("/settings", methods=["POST"])
def settings():
    name = request.form.get("name")
    password = request.form.get("password")
    confirm_password = request.form.get("confirm_password")

    if password and password != confirm_password:
        return redirect(request.referrer or url_for('dashboard'))

    try:
        resp = requests.post(
            f"{API_BASE_URL}/auth/update-profile",
            json={"user_id": session['user_id'], "name": name, "password": password if password else None},
            headers={"X-API-Key": API_KEY},
            timeout=5
        )
        if resp.status_code == 200:
            session['name'] = name
    except Exception:
        pass

    return redirect(request.referrer or url_for('dashboard'))

@app.route("/admin", methods=["GET", "POST"])
def admin_panel():
    if session.get('role') != 'admin':
        return redirect(url_for('dashboard'))

    error = None
    success = None

    if request.method == "POST":
        action = request.form.get("action")

        if action == "create":
            username = request.form.get("username")
            name = request.form.get("name")
            password = request.form.get("password")
            role = request.form.get("role", "user")
            try:
                resp = requests.post(
                    f"{API_BASE_URL}/admin/users",
                    json={"username": username, "name": name, "password": password, "role": role},
                    headers={"X-API-Key": API_KEY},
                    timeout=5
                )
                if resp.status_code == 200:
                    success = "Gebruiker succesvol aangemaakt."
                else:
                    error = resp.json().get("error", "Fout bij aanmaken gebruiker.")
            except Exception:
                error = "Kon niet verbinden met de server."

        elif action == "delete":
            user_id = request.form.get("user_id")
            try:
                resp = requests.delete(
                    f"{API_BASE_URL}/admin/users/{user_id}",
                    headers={"X-API-Key": API_KEY},
                    timeout=5
                )
                if resp.status_code == 200:
                    success = "Gebruiker succesvol verwijderd."
                else:
                    error = "Fout bij verwijderen gebruiker."
            except Exception:
                error = "Kon niet verbinden met de server."

        elif action == "set_fields":
            user_id = request.form.get("user_id")
            field_ids = [int(x) for x in request.form.getlist("fields")]
            try:
                resp = requests.post(
                    f"{API_BASE_URL}/admin/users/{user_id}/fields",
                    json={"fields": field_ids},
                    headers={"X-API-Key": API_KEY},
                    timeout=5
                )
                if resp.status_code == 200:
                    success = "Veldtoegankelijkheid succesvol bijgewerkt."
                else:
                    error = "Fout bij bijwerken veldtoegang."
            except Exception:
                error = "Kon niet verbinden met de server."

    users_list = []
    try:
        resp = requests.get(
            f"{API_BASE_URL}/admin/users",
            headers={"X-API-Key": API_KEY},
            timeout=5
        )
        if resp.status_code == 200:
            users_list = resp.json().get("users", [])
    except Exception:
        error = "Kon gebruikerslijst niet ophalen."

    _, _, fields = get_rows()
    all_fields = []
    for i, f in enumerate(fields):
        name = _attr(f.get("attributes", {}), "Name", "Naam", "Field_Name") or f"Veld {i+1}"
        all_fields.append({"id": i, "name": name})

    return render_template("admin.html", active_page="admin", users=users_list, all_fields=all_fields, error=error, success=success)

@app.route("/logout")
def logout():
    session.clear()
    return redirect(url_for('login'))

# arcgis integration
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
QUALITY_COLORS = ["#60a526", "#84cc16", "#eab308", "#f97316", "#ef4444"]

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

def polygon_centroid(rings):
    if not rings or not rings[0]: return (0.0, 0.0)
    pts = rings[0]
    return (sum(p[0] for p in pts)/len(pts), sum(p[1] for p in pts)/len(pts))

def calculate_growth(history, target_height):
    MOW_BASE_MM = 40
    chron = list(reversed(history))

    if len(chron) < 2:
        return {
            "avg_daily_growth": 0,
            "current_height": chron[0]["h"] if chron else 0,
            "days_to_target": None,
            "expected_date": None,
            "mow_base_mm": MOW_BASE_MM,
        }

    growth_deltas = []
    for i in range(1, len(chron)):
        prev = chron[i - 1]
        curr = chron[i]

        if curr.get("is_mow"):
            continue
        if prev.get("is_mow"):
            delta = curr["h"] - MOW_BASE_MM
        else:
            delta = curr["h"] - prev["h"]

        if delta > 0:
            growth_deltas.append(delta)

    avg_daily_growth = (sum(growth_deltas) / len(growth_deltas)) if growth_deltas else 0
    current_height = chron[-1]["h"]

    if avg_daily_growth <= 0 or current_height >= target_height:
        return {
            "avg_daily_growth": round(avg_daily_growth, 2),
            "current_height": current_height,
            "days_to_target": None,
            "expected_date": None,
            "mow_base_mm": MOW_BASE_MM,
        }

    days_to_target = math.ceil((target_height - current_height) / avg_daily_growth)
    expected_date = (datetime.datetime.now() + datetime.timedelta(days=days_to_target)).strftime("%Y-%m-%d")

    return {
        "avg_daily_growth": round(avg_daily_growth, 2),
        "current_height": current_height,
        "days_to_target": days_to_target,
        "expected_date": expected_date,
        "mow_base_mm": MOW_BASE_MM,
    }

def build_report(rows, fields, days=None, target_field_id=None, user_id=None, user_role=None):
    now = datetime.datetime.now()

    if days:
        try:
            cutoff = now - datetime.timedelta(days=int(days))
            cutoff_str = cutoff.strftime("%Y-%m-%d %H:%M:%S")
            rows = [r for r in rows if str(r.get("measured_at", "")) >= cutoff_str]
        except Exception: pass

    field_summaries = []
    allowed_fields = None
    if user_role == 'user' and user_id is not None:
        allowed_fields = _get_user_fields(user_id)

    for i, f in enumerate(fields):
        if allowed_fields is not None and i not in allowed_fields:
            continue

        name = _attr(f.get("attributes", {}), "Name", "Naam", "Field_Name") or f"Veld {i+1}"
        shape_area = _attr(f.get("attributes", {}), "Shape_Area", "shape_area", "SHAPE_AREA") or 0
        rings = f.get("geometry", {}).get("rings", [])

        # calculate center point for routing
        lon, lat = polygon_centroid(rings)
        f_rows = [r for r in rows if _in_poly(r.get("longitude"), r.get("latitude"), rings)]

        if not f_rows: continue

        # calculate accurate counts of raw measurements for the donut charts
        f_counts = [0, 0, 0, 0, 0]
        for r in f_rows:
            h = r.get("tof_mm") or r.get("sonic_mm") or 0
            f_counts[quality_grade(h)] += 1

        # safe string conversion to avoid 'NoneType' crashes on missing dates
        raw_ts = f_rows[0].get("measured_at")
        measured_at_str = str(raw_ts) if raw_ts else ""
        latest_day = measured_at_str[:10]

        day_rows = [r for r in f_rows if str(r.get("measured_at") or "").startswith(latest_day)]
        f_heights_day = [r.get("tof_mm") or r.get("sonic_mm") or 0 for r in day_rows]
        f_avg = round(sum(f_heights_day) / len(f_heights_day)) if f_heights_day else 0
        lvl = quality_grade(f_avg)

        history_clusters = []
        current_cluster = [f_rows[0]]
        for r in f_rows[1:]:
            last_dt_raw = current_cluster[-1].get("measured_at")
            last_dt_str = str(last_dt_raw) if last_dt_raw else ""
            curr_dt_raw = r.get("measured_at")
            curr_dt_str = str(curr_dt_raw) if curr_dt_raw else ""

            try:
                last_dt = datetime.datetime.strptime(last_dt_str[:16], "%Y-%m-%d %H:%M")
                curr_dt = datetime.datetime.strptime(curr_dt_str[:16], "%Y-%m-%d %H:%M")
                delta = abs((last_dt - curr_dt).total_seconds())
                if delta <= 7200:
                    current_cluster.append(r)
                else:
                    history_clusters.append(current_cluster)
                    current_cluster = [r]
            except Exception:
                if last_dt_str[:10] == curr_dt_str[:10] and last_dt_str[:10] != "":
                    current_cluster.append(r)
                else:
                    history_clusters.append(current_cluster)
                    current_cluster = [r]
        if current_cluster:
            history_clusters.append(current_cluster)

        history = []
        for cluster in history_clusters[:40]:
            c_heights = [(r.get("tof_mm") or r.get("sonic_mm") or 0) for r in cluster]
            c_avg_clus = round(sum(c_heights) / len(c_heights)) if c_heights else 0
            fst_raw = cluster[0].get("measured_at")
            fst = str(fst_raw) if fst_raw else ""

            history.append({
                "date": fst[:10] if len(fst) >= 10 else fst,
                "time": fst[11:16] if len(fst) >= 16 else "",
                "h": c_avg_clus,
                "lvl": quality_grade(c_avg_clus),
                "count": len(cluster)
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

        target_height = _attr(f.get("attributes", {}), "target_height", "Target_Height", "TARGET_HEIGHT") or 40
        growth_info = calculate_growth(history, target_height=target_height)
        action = "Direct" if lvl == 4 else ("Inplannen" if lvl == 3 else "N.v.t.")

        field_summaries.append({
            "id": i,
            "name": name,
            "total": len(f_rows),
            "raw_counts": f_counts,
            "avg": f_avg,
            "latest": str(raw_ts) if raw_ts else "—",
            "label": QUALITY_LABELS[lvl],
            "level": lvl,
            "action": action,
            "bar_pct": min(100, round((f_avg / 150.0) * 100)),
            "history": history,
            "lat": lat,
            "lon": lon,
            "area_m2": shape_area,
            "growth": growth_info,
            "target_height": target_height
        })

    field_summaries.sort(key=lambda s: s["level"], reverse=True)

    if target_field_id is not None and target_field_id != "":
        field_summaries = [s for s in field_summaries if str(s["id"]) == str(target_field_id)]

    counts = [0, 0, 0, 0, 0]
    for s in field_summaries:
        for level_idx in range(5):
            counts[level_idx] += s["raw_counts"][level_idx]

    total = sum(counts)

    # calculate svg values for donut chart
    r_circle = 60
    circ = 2 * math.pi * r_circle
    segments = []
    offset = 0.0
    for lvl_idx, color in enumerate(QUALITY_COLORS):
        frac = (counts[lvl_idx] / total) if total else 0
        seg_len = frac * circ
        segments.append({"color": color, "dash": f"{seg_len:.2f} {circ - seg_len:.2f}", "offset": f"{-offset:.2f}"})
        offset += seg_len

    latest_formatted = "—"
    raw_ts = "—"

    if target_field_id is not None and target_field_id != "" and len(field_summaries) > 0:
        raw_ts = str(field_summaries[0].get("latest", "—"))
    elif rows:
        raw_ts_rows = rows[0].get("measured_at")
        raw_ts = str(raw_ts_rows) if raw_ts_rows else "—"

    if raw_ts != "—" and len(raw_ts) >= 16:
        try:
            dt = datetime.datetime.strptime(raw_ts[:16], "%Y-%m-%d %H:%M")
            latest_formatted = dt.strftime("%d-%m %H:%M")
        except Exception:
            pass

    def safe_avg_sum():
        total_fields = sum([s["total"] for s in field_summaries])
        return round(sum([s["avg"] * s["total"] for s in field_summaries]) / total_fields) if total_fields else 0

    return {
        "total": total,
        "avg": safe_avg_sum(),
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
    if session.get('role') == 'user':
        allowed = _get_user_fields(session.get('user_id'))
        fields = [f for i, f in enumerate(fields) if i in allowed]
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
    rep = build_report(rows, fields, days=days, target_field_id=field_id, user_id=session.get('user_id'), user_role=session.get('role'))
    rep["source"] = source
    return jsonify(rep)

@app.route("/api/route-link")
def api_route_link():
    rows, source, fields = get_rows()
    rep = build_report(rows, fields, days="30", user_id=session.get('user_id'), user_role=session.get('role'))

    # filter fields requiring action (level 3 and 4)
    fields_to_mow = [f for f in rep["fields"] if f.get("level", 0) >= 3]

    if not fields_to_mow:
        return jsonify({"status": "empty", "message": "Geen velden vereisen momenteel een maaibeurt."})

    def priority_score(f):
        growth = f.get("growth", {})
        days_left = growth.get("days_to_target")
        area_m2 = f.get("area_m2", 0)

        MOWING_RATE_M2_PER_HOUR = 5000
        mow_hours = area_m2 / MOWING_RATE_M2_PER_HOUR if area_m2 else 0

        if days_left is None and growth.get("current_height", 0) >= f.get("target_height"):
            return (float("inf"), mow_hours)

        if days_left is None:
            return (float("-inf"), 0)

        effective_days = days_left - (mow_hours / 24)
        return (-effective_days, mow_hours)

    fields_to_mow.sort(key=priority_score, reverse=True)

    url_parts = []
    for f in fields_to_mow:
        safe_name = urllib.parse.quote(str(f["name"])[:128])
        url_parts.append(f"stop={f['lat']},{f['lon']}&stopname={safe_name}")

    # optimize=false ensures strict priority sorting
    navigator_link = "arcgis-navigator://?" + "&".join(url_parts) + "&optimize=false"
    return jsonify({"status": "success", "link": navigator_link})

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

@app.route("/mow")
def mow(): return render_template("mow.html", active_page="mow")

@app.route("/download-pdf")
def download_pdf():
    from playwright.sync_api import sync_playwright
    field_id = request.args.get("field")
    days = request.args.get("days")
    if days == "": days = None
    rows, _, fields = get_rows()

    html_content = render_template("pdf.html", r=build_report(rows, fields, days=days, target_field_id=field_id, user_id=session.get('user_id'), user_role=session.get('role')))

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        page.set_content(html_content, wait_until="networkidle")
        pdf_bytes = page.pdf(
            format="A4",
            print_background=True,
            margin={"top": "18mm", "bottom": "18mm", "left": "14mm", "right": "14mm"}
        )
        browser.close()

    resp = make_response(pdf_bytes)
    resp.headers["Content-Type"] = "application/pdf"
    resp.headers["Content-Disposition"] = "attachment; filename=veldbeheer-rapport.pdf"
    return resp

if __name__ == "__main__":
    app.run(port=3000, debug=True)