import sys
import logging
import mysql.connector
from mysql.connector import Error
from arcgis.gis import GIS
from arcgis.features import FeatureLayer, Feature
from arcgis.geometry import Point

try:
    import credentials
except ModuleNotFoundError:
    sys.exit("ERROR: credentials.py not found. Please create it.")

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s'
)

# determine quality grade based on grass height
def determine_quality_grade(height_mm):
    if height_mm is None:
        return "Unknown"

    if height_mm <= 70:
        return "A+"
    elif height_mm <= 80:
        return "A"
    elif height_mm <= 90:
        return "B"
    elif height_mm <= 100:
        return "C"
    else:
        return "D"

def sync_to_arcgis():
    db = cursor = None
    try:
        # connect to arcgis online
        logging.info("Connecting to ArcGIS Online...")
        gis = GIS("https://www.arcgis.com", credentials.ARCGIS_USERNAME, credentials.ARCGIS_PASSWORD)
        layer = FeatureLayer(credentials.ARCGIS_LAYER_URL)

        # connect to mysql database
        logging.info("Connecting to MySQL Database...")
        db = mysql.connector.connect(
            host=credentials.DB_HOST,
            user=credentials.DB_USER,
            password=credentials.DB_PASSWORD,
            database=credentials.DB_NAME
        )
        cursor = db.cursor(dictionary=True)

        # fetch unsynced readings with valid coordinates
        query = """
            SELECT id, latitude, longitude, tof_mm, sonic_mm, measured_at
            FROM sensor_readings
            WHERE synced = 0
              AND latitude IS NOT NULL
              AND longitude IS NOT NULL
            LIMIT 500
        """
        cursor.execute(query)
        rows = cursor.fetchall()

        if not rows:
            logging.info("No new data to sync. Exiting.")
            return

        logging.info(f"Found {len(rows)} new measurements. Preparing data...")

        features = []
        synced_ids = []

        # process fetched rows
        for row in rows:
            tof_height = row.get("tof_mm")
            sonic_height = row.get("sonic_mm")

            # skip if we have no height data
            if tof_height is None and sonic_height is None:
                continue

            tof_quality = determine_quality_grade(tof_height)
            sonic_quality = determine_quality_grade(sonic_height)

            # create arcgis point feature with separate readings
            feature = Feature(
                geometry=Point({
                    "x": row["longitude"],
                    "y": row["latitude"],
                    "spatialReference": {"wkid": 4326}
                }),
                attributes={
                    "ToF_Height_mm": tof_height,
                    "ToF_Quality_Grade": tof_quality,
                    "Sonic_Height_mm": sonic_height,
                    "Sonic_Quality_Grade": sonic_quality,
                    "Measured_At": str(row["measured_at"])
                }
            )
            features.append(feature)
            synced_ids.append(row["id"])

        if not features:
            logging.info("No valid features with height data to sync.")
            return

        # push features to arcgis
        logging.info(f"Pushing {len(features)} points to ArcGIS...")
        result = layer.edit_features(adds=features)

        # mark records as synced if successful
        if "addResults" in result and len(result["addResults"]) > 0:
            logging.info("Successfully added to ArcGIS.")

            format_strings = ','.join(['%s'] * len(synced_ids))
            update_query = f"UPDATE sensor_readings SET synced = 1 WHERE id IN ({format_strings})"

            cursor.execute(update_query, tuple(synced_ids))
            db.commit()

            logging.info(f"Database updated: {len(synced_ids)} rows marked as synced.")
        else:
            logging.error(f"Failed to add features to ArcGIS. Response: {result}")

    except Error as e:
        logging.error(f"Database error: {e}")
    except Exception as e:
        logging.error(f"Unexpected error: {e}")
    finally:
        # cleanup connections
        if cursor:
            cursor.close()
        if db and db.is_connected():
            db.close()

if __name__ == "__main__":
    sync_to_arcgis()