from flask import Flask, request, jsonify
from flask_limiter import Limiter
from flask_limiter.util import get_remote_address
from mysql.connector import pooling, Error
from werkzeug.security import generate_password_hash, check_password_hash
from datetime import datetime, timezone
import sys
import struct
import math
import hmac
import logging

try:
    import credentials
except ModuleNotFoundError:
    sys.exit("ERROR: credentials.py not found.")

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s'
)

# init flask app
app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 1 * 1024 * 1024

limiter = Limiter(
    key_func=get_remote_address,
    app=app,
    storage_uri="memory://",
    default_limits=["300 per minute"],
)

db_pool = pooling.MySQLConnectionPool(
    pool_name="sensor_pool",
    pool_size=5,
    host=credentials.DB_HOST,
    user=credentials.DB_USER,
    password=credentials.DB_PASSWORD,
    database=credentials.DB_NAME,
)

DEFAULT_LIMIT = 100
MAX_LIMIT = 5000

def get_db():
    return db_pool.get_connection()

# format datetime objects for json response
def serialize_row(row: dict) -> dict:
    for key, val in row.items():
        if isinstance(val, datetime):
            row[key] = val.strftime("%Y-%m-%dT%H:%M:%SZ")
    return row

def safe_float(val):
    if val is None: return None
    try: return float(val) if not math.isnan(float(val)) else None
    except (ValueError, TypeError): return None

def safe_int(val):
    if val is None: return None
    try: return int(val)
    except (ValueError, TypeError): return None

# check api key against credentials
def verify_api_key(req):
    provided_key = req.headers.get("X-API-Key", "")
    if provided_key is None:
        provided_key = ""
    return hmac.compare_digest(provided_key, credentials.API_KEY)

# health check
@app.route("/", methods=["GET"])
def health_check():
    return jsonify({
        "service": "groenwerf-api",
        "status": "online",
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }), 200


# Authentication Endpoints

@app.route("/auth/register", methods=["POST"])
@limiter.limit("10 per minute")
def register():
    if not verify_api_key(request):
        return jsonify({"error": "unauthorized"}), 401

    data = request.get_json(silent=True) or {}
    username = data.get("username")
    password = data.get("password")
    name = data.get("name")
    invite_code = data.get("invite_code")

    if not all([username, password, name, invite_code]):
        return jsonify({"error": "Ontbrekende velden"}), 400

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor(dictionary=True)

        # check invite code validity
        cursor.execute("SELECT id, is_used FROM invite_codes WHERE code = %s", (invite_code,))
        code_row = cursor.fetchone()
        if not code_row:
            return jsonify({"error": "Ongeldige invite code"}), 400
        if code_row["is_used"]:
            return jsonify({"error": "Deze invite code is al gebruikt"}), 400

        # check if username already exists
        cursor.execute("SELECT id FROM users WHERE username = %s", (username,))
        if cursor.fetchone():
            return jsonify({"error": "Gebruikersnaam is al in gebruik"}), 400

        # hash password and insert user
        hashed_pw = generate_password_hash(password)
        cursor.execute("INSERT INTO users (username, password_hash, name, role) VALUES (%s, %s, %s, 'user')", (username, hashed_pw, name))

        # mark invite code as used
        cursor.execute("UPDATE invite_codes SET is_used = 1 WHERE id = %s", (code_row["id"],))
        db.commit()

        return jsonify({"status": "ok"}), 200

    except Error as e:
        if db: db.rollback()
        logging.error(f"DB Error in register: {e}")
        return jsonify({"error": "database error"}), 500
    finally:
        if cursor: cursor.close()
        if db and db.is_connected(): db.close()

@app.route("/auth/login", methods=["POST"])
@limiter.limit("30 per minute")
def login():
    if not verify_api_key(request):
        return jsonify({"error": "unauthorized"}), 401

    data = request.get_json(silent=True) or {}
    username = data.get("username")
    password = data.get("password")

    if not username or not password:
        return jsonify({"error": "Vul alle velden in"}), 400

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor(dictionary=True)
        cursor.execute("SELECT id, name, password_hash, role FROM users WHERE username = %s", (username,))
        user = cursor.fetchone()

        if user and check_password_hash(user["password_hash"], password):
            return jsonify({"status": "ok", "user": {"id": user["id"], "name": user["name"], "username": username, "role": user["role"]}}), 200
        else:
            return jsonify({"error": "Ongeldige inloggegevens"}), 401

    except Error as e:
        logging.error(f"DB Error in login: {e}")
        return jsonify({"error": "database error"}), 500
    finally:
        if cursor: cursor.close()
        if db and db.is_connected(): db.close()

@app.route("/auth/update-profile", methods=["POST"])
@limiter.limit("15 per minute")
def update_profile():
    if not verify_api_key(request):
        return jsonify({"error": "unauthorized"}), 401

    data = request.get_json(silent=True) or {}
    user_id = data.get("user_id")
    name = data.get("name")
    password = data.get("password")

    if not user_id or not name:
        return jsonify({"error": "Ontbrekende velden"}), 400

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor()

        if password:
            hashed_pw = generate_password_hash(password)
            cursor.execute("UPDATE users SET name = %s, password_hash = %s WHERE id = %s", (name, hashed_pw, user_id))
        else:
            cursor.execute("UPDATE users SET name = %s WHERE id = %s", (name, user_id))

        db.commit()
        return jsonify({"status": "ok"}), 200
    except Error as e:
        if db: db.rollback()
        logging.error(f"DB Error in update_profile: {e}")
        return jsonify({"error": "database error"}), 500
    finally:
        if cursor: cursor.close()
        if db and db.is_connected(): db.close()


# Admin User Management Endpoints

@app.route("/admin/users", methods=["GET"])
def admin_get_users():
    if not verify_api_key(request):
        return jsonify({"error": "unauthorized"}), 401

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor(dictionary=True)
        cursor.execute("SELECT id, username, name, role, created_at FROM users")
        users = cursor.fetchall()

        for u in users:
            if isinstance(u["created_at"], datetime):
                u["created_at"] = u["created_at"].strftime("%Y-%m-%d %H:%M:%S")

            # fetch allowed fields for each user
            cursor.execute("SELECT field_id FROM user_fields WHERE user_id = %s", (u["id"],))
            u["fields"] = [r["field_id"] for r in cursor.fetchall()]

        return jsonify({"users": users}), 200
    except Error as e:
        logging.error(f"DB Error in admin_get_users: {e}")
        return jsonify({"error": "database error"}), 500
    finally:
        if cursor: cursor.close()
        if db and db.is_connected(): db.close()

@app.route("/admin/users", methods=["POST"])
def admin_create_user():
    if not verify_api_key(request):
        return jsonify({"error": "unauthorized"}), 401

    data = request.get_json(silent=True) or {}
    username = data.get("username")
    password = data.get("password")
    name = data.get("name")
    role = data.get("role", "user")

    if not all([username, password, name]):
        return jsonify({"error": "Vul alle verplichte velden in"}), 400

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor(dictionary=True)

        cursor.execute("SELECT id FROM users WHERE username = %s", (username,))
        if cursor.fetchone():
            return jsonify({"error": "Gebruikersnaam bestaat al"}), 400

        hashed_pw = generate_password_hash(password)
        cursor.execute("INSERT INTO users (username, password_hash, name, role) VALUES (%s, %s, %s, %s)", (username, hashed_pw, name, role))
        db.commit()

        return jsonify({"status": "ok"}), 200
    except Error as e:
        if db: db.rollback()
        logging.error(f"DB Error in admin_create_user: {e}")
        return jsonify({"error": "database error"}), 500
    finally:
        if cursor: cursor.close()
        if db and db.is_connected(): db.close()

@app.route("/admin/users/<int:user_id>", methods=["DELETE"])
def admin_delete_user():
    if not verify_api_key(request):
        return jsonify({"error": "unauthorized"}), 401

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor()
        cursor.execute("DELETE FROM users WHERE id = %s", (user_id,))
        db.commit()
        return jsonify({"status": "ok"}), 200
    except Error as e:
        if db: db.rollback()
        logging.error(f"DB Error in admin_delete_user: {e}")
        return jsonify({"error": "database error"}), 500
    finally:
        if cursor: cursor.close()
        if db and db.is_connected(): db.close()

@app.route("/admin/users/<int:user_id>/fields", methods=["POST"])
def admin_set_user_fields(user_id):
    if not verify_api_key(request):
        return jsonify({"error": "unauthorized"}), 401

    data = request.get_json(silent=True) or {}
    fields = data.get("fields", [])

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor()

        cursor.execute("DELETE FROM user_fields WHERE user_id = %s", (user_id,))
        if fields:
            values = [(user_id, int(f_id)) for f_id in fields]
            cursor.executemany("INSERT INTO user_fields (user_id, field_id) VALUES (%s, %s)", values)

        db.commit()
        return jsonify({"status": "ok"}), 200
    except Error as e:
        if db: db.rollback()
        logging.error(f"DB Error in admin_set_user_fields: {e}")
        return jsonify({"error": "database error"}), 500
    finally:
        if cursor: cursor.close()
        if db and db.is_connected(): db.close()

@app.route("/users/<int:user_id>/fields", methods=["GET"])
def get_user_fields(user_id):
    if not verify_api_key(request):
        return jsonify({"error": "unauthorized"}), 401

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor(dictionary=True)
        cursor.execute("SELECT field_id FROM user_fields WHERE user_id = %s", (user_id,))
        rows = cursor.fetchall()
        fields = [r["field_id"] for r in rows]
        return jsonify({"fields": fields}), 200
    except Error as e:
        logging.error(f"DB Error in get_user_fields: {e}")
        return jsonify({"error": "database error"}), 500
    finally:
        if cursor: cursor.close()
        if db and db.is_connected(): db.close()


# Sensor Endpoints

@app.route("/sensor-data", methods=["POST"])
@limiter.limit("120 per minute")
def receive_data():
    if not verify_api_key(request):
        return jsonify({"error": "unauthorized"}), 401

    db = cursor = None
    inserted = 0
    fallback_time = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
    values_to_insert = []

    try:
        db = get_db()
        cursor = db.cursor()

        # parse binary payload
        if request.content_type == "application/octet-stream":
            raw_data = request.get_data(as_text=False)
            item_size = 40

            # validate payload size
            if len(raw_data) % item_size != 0 or len(raw_data) == 0:
                return jsonify({"error": "invalid binary payload length"}), 400

            if (len(raw_data) // item_size) > 100:
                return jsonify({"error": "batch too large, maximum 100 items per request"}), 400

            # process binary chunks
            for i in range(0, len(raw_data), item_size):
                chunk = raw_data[i:i+item_size]
                unpacked = struct.unpack("<ffHHfHHfffHBBBBBB", chunk)

                lat = unpacked[0] if not math.isnan(unpacked[0]) else None
                lon = unpacked[1] if not math.isnan(unpacked[1]) else None
                ght = unpacked[2] if unpacked[2] != 0xFFFF else None
                ghs = unpacked[3] if unpacked[3] != 0xFFFF else None
                t   = unpacked[4] if not math.isnan(unpacked[4]) else None
                sr  = unpacked[5] if unpacked[5] != 0xFFFF else None
                tr  = unpacked[6] if unpacked[6] != 0xFFFF else None
                ax  = unpacked[7] if not math.isnan(unpacked[7]) else None
                ay  = unpacked[8] if not math.isnan(unpacked[8]) else None
                az  = unpacked[9] if not math.isnan(unpacked[9]) else None
                year, month, day, hour, minute, second, valid_time = unpacked[10:17]

                # use gps time if valid, else fallback
                if valid_time == 1:
                    measured_at = f"{year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}"
                else:
                    measured_at = fallback_time

                values_to_insert.append((lat, lon, ght, ghs, t, sr, tr, ax, ay, az, measured_at))

        # parse json payload
        else:
            data = request.get_json(silent=True)
            if not data or not isinstance(data, list):
                return jsonify({"error": "expected a JSON array"}), 400

            if len(data) > 100:
                return jsonify({"error": "batch too large, maximum 100 items per request"}), 400

            for item in data:
                if not isinstance(item, dict):
                    continue

                measured_at = item.get("measured_at", item.get("m_at"))
                if measured_at and isinstance(measured_at, str):
                    try:
                        measured_at = measured_at.replace("T", " ").replace("Z", "")
                    except (ValueError, TypeError, AttributeError):
                        measured_at = fallback_time
                else:
                    measured_at = fallback_time

                values_to_insert.append((
                    safe_float(item.get("lat", item.get("lt"))),
                    safe_float(item.get("lon", item.get("ln"))),
                    safe_int(item.get("grassHeightTof", item.get("ght"))),
                    safe_int(item.get("grassHeightSonic", item.get("ghs"))),
                    safe_float(item.get("temperature", item.get("t"))),
                    safe_int(item.get("sonic_raw_mm", item.get("sr"))),
                    safe_int(item.get("tof_raw_mm", item.get("tr"))),
                    safe_float(item.get("accel_raw_x", item.get("ax"))),
                    safe_float(item.get("accel_raw_y", item.get("ay"))),
                    safe_float(item.get("accel_raw_z", item.get("az"))),
                    measured_at,
                ))

        # insert valid readings
        if values_to_insert:
            insert_query = """
                INSERT INTO sensor_readings
                    (latitude, longitude, tof_mm, sonic_mm, temperature,
                     sonic_raw_mm, tof_raw_mm,
                     accel_raw_x, accel_raw_y, accel_raw_z, measured_at)
                VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            """
            cursor.executemany(insert_query, values_to_insert)
            db.commit()
            inserted = len(values_to_insert)

        return jsonify({"status": "ok", "inserted": inserted}), 200

    except struct.error as e:
        logging.warning(f"Binary parsing error: {e}")
        return jsonify({"error": "invalid binary structure"}), 400
    except Error as e:
        if db:
            db.rollback()
        logging.error(f"Database error in receive_data: {e}")
        return jsonify({"error": "db error"}), 500
    except Exception as e:
        if db:
            db.rollback()
        logging.error(f"Unexpected error in receive_data: {e}", exc_info=True)
        return jsonify({"error": "internal server error"}), 500
    finally:
        if cursor:
            try: cursor.close()
            except Exception: pass
        if db and db.is_connected():
            db.close()

# retrieve sensor data
@app.route("/sensor-data", methods=["GET"])
@limiter.limit("60 per minute")
def get_data():
    if not verify_api_key(request):
        return jsonify({"error": "unauthorized"}), 401

    # parse pagination args
    try:
        limit = int(request.args.get("limit", DEFAULT_LIMIT))
        offset = int(request.args.get("offset", 0))
    except ValueError:
        return jsonify({"error": "limit and offset must be integers"}), 400

    if limit < 1 or limit > MAX_LIMIT:
        return jsonify({"error": f"limit must be between 1 and {MAX_LIMIT}"}), 400
    if offset < 0:
        return jsonify({"error": "offset must be >= 0"}), 400

    order = request.args.get("order", "desc").lower()
    if order not in ("asc", "desc"):
        return jsonify({"error": "order must be 'asc' or 'desc'"}), 400

    filters = []
    params = []

    # parse time filters
    from_str = request.args.get("from")
    if from_str:
        try:
            from_dt = datetime.strptime(from_str, "%Y-%m-%dT%H:%M:%SZ")
            filters.append("measured_at >= %s")
            params.append(from_dt.strftime("%Y-%m-%d %H:%M:%S"))
        except ValueError:
            return jsonify({"error": "invalid 'from' format"}), 400

    to_str = request.args.get("to")
    if to_str:
        try:
            to_dt = datetime.strptime(to_str, "%Y-%m-%dT%H:%M:%SZ")
            filters.append("measured_at <= %s")
            params.append(to_dt.strftime("%Y-%m-%d %H:%M:%S"))
        except ValueError:
            return jsonify({"error": "invalid 'to' format"}), 400

    where_clause = ("WHERE " + " AND ".join(filters)) if filters else ""
    order_clause = f"ORDER BY measured_at {order.upper()}, id {order.upper()}"

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor(dictionary=True)

        # get total count
        cursor.execute(
            f"SELECT COUNT(*) AS total FROM sensor_readings {where_clause}",
            params,
        )
        total = cursor.fetchone()["total"]

        # get requested rows
        cursor.execute(
            f"SELECT * FROM sensor_readings {where_clause} {order_clause} LIMIT %s OFFSET %s",
            params + [limit, offset],
        )
        rows = [serialize_row(row) for row in cursor.fetchall()]

        return jsonify({
            "data": rows,
            "meta": {
                "total": total,
                "limit": limit,
                "offset": offset,
                "returned": len(rows),
            },
        }), 200

    except Error as e:
        logging.error(f"Database error in get_data: {e}")
        return jsonify({"error": "db error"}), 500
    except Exception as e:
        logging.error(f"Unexpected error in get_data: {e}", exc_info=True)
        return jsonify({"error": "internal server error"}), 500
    finally:
        if cursor:
            try: cursor.close()
            except Exception: pass
        if db and db.is_connected():
            db.close()

if __name__ == "__main__":
    app.run(host=credentials.FLASK_HOST, port=credentials.FLASK_PORT)