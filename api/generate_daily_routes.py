from datetime import datetime
import time
from arcgis.gis import GIS
from arcgis.features import Feature, FeatureLayer, FeatureLayerCollection
from arcgis.network import RouteLayer as NetworkRouteLayer
from arcgis.geometry import Point
import credentials

def main():
    print("Connecting to ArcGIS Online...")
    gis = GIS(credentials.ARCGIS_URL, credentials.ARCGIS_USERNAME, credentials.ARCGIS_PASSWORD)

    # ------------------------------------------------------------------
    # 1. Fetch fields and compute centroids
    # ------------------------------------------------------------------
    print("Fetching target fields...")
    field_layer = FeatureLayer(url=credentials.ARCGIS_FIELDS_URL, gis=gis)
    query_result = field_layer.query(where="1=1", out_sr="4326", return_geometry=True).features

    extracted_fields = []
    for feat in query_result:
        field_id = feat.attributes.get("field_id") or f"ID-{feat.attributes.get('OBJECTID')}"
        geom = feat.geometry
        if not geom or 'rings' not in geom:
            continue
        pts = geom['rings'][0]
        lon = sum(p[0] for p in pts) / len(pts)
        lat = sum(p[1] for p in pts) / len(pts)
        extracted_fields.append({
            "id":       field_id,
            "priority": feat.attributes.get("priority") or 999,
            "lat":      lat,
            "lon":      lon,
        })

    sorted_fields = sorted(extracted_fields, key=lambda x: x['priority'])
    print(f"Found {len(sorted_fields)} fields: {[f['id'] for f in sorted_fields]}")

    # ------------------------------------------------------------------
    # 2. Solve route
    # ------------------------------------------------------------------
    print("Computing route...")
    stops_str = ";".join([f"{f['lon']},{f['lat']}" for f in sorted_fields])

    route_service = NetworkRouteLayer(gis.properties.helperServices.route.url, gis=gis)
    result = route_service.solve(
        stops=stops_str,
        find_best_sequence=False,
        return_routes=True,
        out_sr=4326,
    )
    raw_route_geom = result['routes']['features'][0]['geometry']
    raw_route_geom["spatialReference"] = {"wkid": 4326}

    # ------------------------------------------------------------------
    # 3. Create Feature Service + schema
    # ------------------------------------------------------------------
    today_str = datetime.now().strftime("%Y_%m_%d_%H%M")
    layer_name = f"Maairoute_{today_str}"
    print(f"Creating service '{layer_name}'...")

    new_item = gis.content.create_service(
        name=layer_name,
        service_description="Automated routing layer for field navigation.",
        has_static_data=False,
        capabilities="Query,Editing,Create,Update,Delete",
    )

    layer_schema = {
        "layers": [
            {
                "id": 0,
                "name": "Routes",
                "type": "Feature Layer",
                "geometryType": "esriGeometryPolyline",
                "spatialReference": {"wkid": 4326},
                "fields": [
                    {"name": "OBJECTID", "type": "esriFieldTypeOID",    "alias": "OBJECTID",   "nullable": False, "editable": False},
                    {"name": "RouteName","type": "esriFieldTypeString", "alias": "Route Name", "length": 1024},
                ],
            },
            {
                "id": 1,
                "name": "Stops",
                "type": "Feature Layer",
                "geometryType": "esriGeometryPoint",
                "spatialReference": {"wkid": 4326},
                "fields": [
                    {"name": "OBJECTID",  "type": "esriFieldTypeOID",    "alias": "OBJECTID",       "nullable": False, "editable": False},
                    {"name": "Sequence",  "type": "esriFieldTypeInteger", "alias": "Sequence Order"},
                    {"name": "RouteName", "type": "esriFieldTypeString",  "alias": "Assigned Route", "length": 1024},
                    {"name": "StopName",  "type": "esriFieldTypeString",  "alias": "Stop Name",      "length": 1024},
                ],
            },
        ]
    }

    flc = FeatureLayerCollection.fromitem(new_item)
    flc.manager.add_to_definition(layer_schema)

    print(len(flc.layers))

    print("Waiting for endpoints to initialize...")
    time.sleep(8)

    # ------------------------------------------------------------------
    # 4. Write route line (layer 0)
    # ------------------------------------------------------------------
    print("Writing route line...")
    routes_sublayer = FeatureLayer(url=f"{new_item.url}/0", gis=gis)

    route_name = f"Route {datetime.now().strftime('%Y-%m-%d %H:%M')}"
    route_feat = Feature(
        geometry=raw_route_geom,
        attributes={"RouteName": route_name}
    )
    print("route_feat", route_feat)
    routes_response = routes_sublayer.edit_features(adds=[route_feat])
    print(f"Routes response: {routes_response}")
    count = routes_sublayer.query(
        where="1=1",
        return_count_only=True
    )       

    print("Route count:", count)
    for r in routes_response.get("addResults", []):
        if not r.get("success"):
            print(f"  ERROR on route insert: {r}")

    # ------------------------------------------------------------------
    # 5. Write stops (layer 1)
    # ------------------------------------------------------------------
    print("Writing stop points...")
    stops_sublayer = FeatureLayer(url=f"{new_item.url}/1", gis=gis)

    stop_list = []
    for index, f in enumerate(sorted_fields, start=1):
        stop_list.append(Feature(
            geometry=Point({
                "x": f["lon"],
                "y": f["lat"],
                "spatialReference": {"wkid": 4326}
            }),
            attributes={
                "Sequence":  index,
                "RouteName": route_name,
                "StopName":  str(f.get("id", f"Stop {index}")),
            }
        ))

    print(f"Sample stop: {stop_list[0].as_dict}")
    stops_response = stops_sublayer.edit_features(adds=stop_list)
    check = stops_sublayer.query(where="1=1")
    print(check.features)
    print(f"Stops response: {stops_response}")
    
    for r in stops_response.get("addResults", []):
        if not r.get("success"):
            print(f"  ERROR on stop insert: {r}")
    
    for field in stops_sublayer.properties.fields:
        print(field["name"], field["type"])

    for field in routes_sublayer.properties.fields:
        print(field["name"], field["type"])

    for field in routes_sublayer.properties.fields:
        print(field["name"], field["type"])

    print("\n--------------------------------------------------")
    print(f"SUCCESS: {layer_name}")
    print(f"REST Base URL: {new_item.url}")
    print("--------------------------------------------------")


if __name__ == "__main__":
    main()