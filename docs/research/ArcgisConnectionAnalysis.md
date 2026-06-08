# MySQL database importeren naar ArcGIS

## 1: Setup ArcGIS
- Point layer aanmaken in ArcGIS, met fields grass_height en timestamp, latitude en longitude zijn
niet nodig omdat deze al in de point layer ingebouwd zitten.

## 2: MySQL data importeren
- Python script om de data uit arcgis te importeren
- Dit script maakt verbinding met de MySQL database om vervolgens de data te versturen naar de ArcGIS point layer
- Voorbeeld python script om MySQL data naar ArcGIS te versturen

```py
import mysql.connector
from arcgis.gis import GIS
from arcgis.features import FeatureLayer, Feature
from arcgis.geometry import Point


gis = GIS("https://www.arcgis.com", "your_username", "your_password")


layer = FeatureLayer("https://services2.arcgis.com/.../FeatureServer/0")



conn = mysql.connector.connect(
    host="your_host",
    user="your_user",
    password="your_password",
    database="your_database"
)
cursor = conn.cursor(dictionary=True)


cursor.execute("SELECT grass_height, latitude, longitude, timestamp FROM your_table")
rows = cursor.fetchall()


features = []
for row in rows:
    feature = Feature(
        geometry=Point({
            "x": row["longitude"],
            "y": row["latitude"],
            "spatialReference": {"wkid": 4326}
        }),
        attributes={
            "Grass_Height": row["grass_height"],
            "Measured_at": str(row["timestamp"])
        }
    )
    features.append(feature)


layer.edit_features(adds=features)

print(f"Added {len(features)} features!")

cursor.close()
conn.close()
```
