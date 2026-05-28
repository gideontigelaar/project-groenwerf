from flask import Flask, request, jsonify
from flask_limiter import Limiter
from flask_limiter.util import get_remote_address
from mysql.connector import pooling, Error
from datetime import datetime, timezone
import sys

try:
    import credentials
except ModuleNotFoundError:
    sys.exit("ERROR: credentials.py not found.")

app = Flask(__name__)

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


def serialize_row(row: dict) -> dict:
    for key, val in row.items():
        if isinstance(val, datetime):
            row[key] = val.strftime("%Y-%m-%dT%H:%M:%SZ")
    return row


@app.route("/", methods=["GET"])
def health_check():
    return jsonify({
        "service": "groenwerf-api",
        "status": "online",
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }), 200


@app.route("/sensor-data", methods=["POST"])
@limiter.limit("120 per minute")
def receive_data():
    if request.headers.get("X-API-Key") != credentials.API_KEY:
        return jsonify({"error": "unauthorized"}), 401

    data = request.get_json(silent=True)
    if not data or not isinstance(data, list):
        return jsonify({"error": "expected a JSON array"}), 400

    if len(data) > 100:
        return jsonify({"error": "batch too large, maximum 100 items per request"}), 400

    db = cursor = None
    inserted = 0

    try:
        db = get_db()
        cursor = db.cursor()

        values_to_insert = []

        for item in data:
            if not isinstance(item, dict):
                continue

            measured_at = item.get("measured_at")
            if measured_at:
                try:
                    measured_at = measured_at.replace("T", " ").replace("Z", "")
                except (ValueError, TypeError):
                    measured_at = None

            if measured_at is None:
                measured_at = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")

            values_to_insert.append((
                item.get("lat"),
                item.get("lon"),
                item.get("grassHeightTof"),
                item.get("grassHeightSonic"),
                item.get("temperature"),
                item.get("sonic_raw_mm"),
                item.get("tof_raw_mm"),
                item.get("accel_raw_x"),
                item.get("accel_raw_y"),
                item.get("accel_raw_z"),
                measured_at,
            ))

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

    except Error as e:
        if db:
            db.rollback()
        return jsonify({"error": "db error", "details": str(e)}), 500
    finally:
        if cursor:
            cursor.close()
        if db and db.is_connected():
            db.close()


@app.route("/sensor-data", methods=["GET"])
@limiter.limit("60 per minute")
def get_data():
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

    from_str = request.args.get("from")
    if from_str:
        try:
            from_dt = datetime.strptime(from_str, "%Y-%m-%dT%H:%M:%SZ")
            filters.append("measured_at >= %s")
            params.append(from_dt.strftime("%Y-%m-%d %H:%M:%S"))
        except ValueError:
            return jsonify({"error": "invalid 'from' — use ISO 8601: YYYY-MM-DDTHH:MM:SSZ"}), 400

    to_str = request.args.get("to")
    if to_str:
        try:
            to_dt = datetime.strptime(to_str, "%Y-%m-%dT%H:%M:%SZ")
            filters.append("measured_at <= %s")
            params.append(to_dt.strftime("%Y-%m-%d %H:%M:%S"))
        except ValueError:
            return jsonify({"error": "invalid 'to' — use ISO 8601: YYYY-MM-DDTHH:MM:SSZ"}), 400

    where_clause = ("WHERE " + " AND ".join(filters)) if filters else ""
    order_clause = f"ORDER BY measured_at {order.upper()}, id {order.upper()}"

    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor(dictionary=True)

        cursor.execute(
            f"SELECT COUNT(*) AS total FROM sensor_readings {where_clause}",
            params,
        )
        total = cursor.fetchone()["total"]

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
        return jsonify({"error": "db error", "details": str(e)}), 500
    finally:
        if cursor:
            cursor.close()
        if db and db.is_connected():
            db.close()


if __name__ == "__main__":
    app.run(host=credentials.FLASK_HOST, port=credentials.FLASK_PORT, debug=True)