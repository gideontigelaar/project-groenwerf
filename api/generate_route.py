import os
import tempfile
import requests
import logging
from datetime import datetime

from arcgis.gis import GIS
from arcgis.features import FeatureLayer, Feature, FeatureSet
from arcgis.network.analysis import find_routes
from arcgis.features.manage_data import create_route_layers

import credentials

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s'
)

def polygon_area(rings):
    if not rings: return 0.0
    ring = rings[0]
    n = len(ring)
    area = sum(ring[i][0] * ring[(i+1)%n][1] - ring[(i+1)%n][0] * ring[i][1] for i in range(n))
    return abs(area) / 2.0

def polygon_centroid(rings):
    pts = rings[0]
    return (sum(p[0] for p in pts)/len(pts), sum(p[1] for p in pts)/len(pts))

def _rest_add_item(gis, title, item_type, tags, filepath, filename):
    # bypass arcgis geoenabled upload problems
    token = gis._con.token
    url = f"https://www.arcgis.com/sharing/rest/content/users/{gis.properties.user.username}/addItem"

    with open(filepath, "rb") as f:
        res = requests.post(url, data={
            "f": "json", "token": token, "title": title,
            "type": item_type, "tags": tags, "filename": filename
        }, files={"file": (filename, f)})

    res.raise_for_status()
    data = res.json()
    if not data.get("success"):
        raise RuntimeError(f"upload failed: {data}")
    return data["id"]

def main():
    logging.info("connecting to arcgis online...")
    gis = GIS("https://www.arcgis.com", credentials.ARCGIS_USERNAME, credentials.ARCGIS_PASSWORD)

    logging.info("fetching grass fields...")
    layer = FeatureLayer(url=credentials.ARCGIS_FIELDS_URL, gis=gis)
    features = layer.query(where="1=1", out_sr="4326", return_geometry=True).features

    fields = []
    for f in features:
        attr, geom = f.attributes or {}, f.geometry or {}
        rings = geom.get("rings", [])
        if not rings: continue

        lon, lat = polygon_centroid(rings)
        name = attr.get("Name") or attr.get("Naam") or f"field {attr.get('OBJECTID')}"
        prio = attr.get("priority") or attr.get("Priority")

        fields.append({
            "name": name,
            "target_height": float(attr.get("target_height") or attr.get("TargetHeight") or 999),
            "area": polygon_area(rings),
            "prio": int(prio) if prio is not None else 999,
            "lat": lat, "lon": lon
        })

    if not fields:
        logging.warning("no fields found.")
        return

    # sort by priority sequence
    fields.sort(key=lambda x: (x["prio"], x["target_height"], -x["area"]))
    logging.info(f"found {len(fields)} fields, sorted by priority.")

    # consistent capitalisation for arcgis naming conventions
    route_name = f"Maairoute_{datetime.now().strftime('%Y_%m_%d_%H%M')}"
    stops = []

    # generate stops (navigator auto-routes from current physical location to sequence 1)
    for i, f in enumerate(fields, start=1):
        stops.append(Feature(
            geometry={"x": f["lon"], "y": f["lat"], "spatialReference": {"wkid": 4326}},
            attributes={"Name": str(f["name"])[:128], "RouteName": route_name, "Sequence": i}
        ))

    fset = FeatureSet(features=stops, geometry_type="esriGeometryPoint", spatial_reference={"wkid": 4326})

    logging.info("solving route...")
    res = find_routes(
        stops=fset,
        reorder_stops_to_find_optimal_routes=False,
        return_to_start=False,
        save_route_data=True,
        gis=gis
    )

    data_item = getattr(res, 'output_route_data', None) or getattr(res, 'out_route_data', None)
    if not data_item:
        raise RuntimeError("route packaging failed")

    local_zip = temp_item = None

    # download zip if returned as unhosted datafile
    if type(data_item).__name__ == 'DataFile':
        logging.info("downloading temporary route data...")
        data_item = data_item.download(tempfile.gettempdir())

    logging.info("publishing native route layer...")

    # upload raw zip via rest
    if isinstance(data_item, str) and os.path.exists(data_item):
        local_zip = data_item
        item_id = _rest_add_item(gis, f"temp_{route_name}", "File Geodatabase", "temp", local_zip, os.path.basename(local_zip))
        temp_item = gis.content.get(item_id)
        data_item = temp_item

    try:
        # publish native arcgis route layer
        layers = create_route_layers(data_item, gis=gis)
        if isinstance(layers, list) and layers:
            logging.info(f"route published successfully: https://www.arcgis.com/home/item.html?id={layers[0].id}")
    except Exception as e:
        logging.error(f"failed to create route layers: {e}")
    finally:
        if temp_item:
            temp_item.delete()
            logging.info("cleaned up temporary agol item.")
        if local_zip and os.path.exists(local_zip):
            os.remove(local_zip)
            logging.info("cleaned up local temporary zip.")

if __name__ == "__main__":
    main()