(async function () {
    const fieldsUrl = window.ARCGIS_FIELDS_URL;
    const emptyState = document.getElementById("mapEmpty");

    if (!fieldsUrl) {
        emptyState.classList.remove("hidden");
        document.getElementById("fields-count").textContent = "Configuratiefout";
        return;
    }

    // init leaflet map
    const map = L.map('mapView', { zoomControl: false }).setView([52.1, 5.2], 7); // default nl center
    L.control.zoom({ position: 'bottomright' }).addTo(map);

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19,
        attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>'
    }).addTo(map);

    let allPoints = [];
    let fieldsGeoJSON = null;
    let fieldLayerGroup = L.featureGroup().addTo(map);
    let pointLayerGroup = L.featureGroup().addTo(map);

    function colorFor(h) {
        return h >= 400 ? "#ef4444" : h >= 80 ? "#f59e0b" : "#6aa84f";
    }

    try {
        // fetch fields via proxy
        const fieldsRes = await fetch("/api/fields");

        if (!fieldsRes.ok) {
            throw new Error(`Kon velden niet laden via proxy (HTTP ${fieldsRes.status})`);
        }

        const esriData = await fieldsRes.json();

        if (esriData.error) {
            console.error("Backend Proxy Fout:", esriData.error);
            throw new Error("Backend kon de velden niet ophalen: " + esriData.error);
        }

        // convert esri json to geojson
        fieldsGeoJSON = {
            type: "FeatureCollection",
            features: (esriData.features || []).map(f => {
                return {
                    type: "Feature",
                    properties: f.attributes || {},
                    geometry: (f.geometry && f.geometry.rings) ? {
                        type: "Polygon",
                        coordinates: f.geometry.rings
                    } : null
                };
            }).filter(f => f.geometry != null)
        };

        // fetch measurements
        const dataRes = await fetch("/api/data");
        const dataJson = await dataRes.json();
        const rows = dataJson.data || [];

        // convert to turf points
        allPoints = rows.filter(r => r.longitude != null && r.latitude != null).map((r, i) => {
            return turf.point([r.longitude, r.latitude], { ...r, id: i, h: r.tof_mm || r.sonic_mm || 0 });
        });

        // populate dropdown
        const sel = document.getElementById("fieldSelect");
        const dateFilter = document.getElementById("dateFilter");
        const sortSelect = document.getElementById("sortSelect");

        sel.innerHTML = '<option value="">Kies een veld…</option>';

        let validFieldsCount = 0;

        fieldsGeoJSON.features.forEach((f, i) => {
            if (!f.geometry) return; // skip without geometry
            validFieldsCount++;

            const name = f.properties.Name || f.properties.Naam || f.properties.Field_Name || `Veld ${i + 1}`;
            f.properties._id = i;

            const opt = document.createElement("option");
            opt.value = i;
            opt.textContent = name;
            sel.appendChild(opt);

            // draw polygon
            const layer = L.geoJSON(f, {
                style: { color: "#3b6d11", weight: 2, fillOpacity: 0.1 }
            }).addTo(fieldLayerGroup);

            f.properties._layer = layer;
        });

        document.getElementById("fields-count").textContent = validFieldsCount + " veld" + (validFieldsCount === 1 ? "" : "en");

        // fit bounds to all fields
        if (validFieldsCount > 0) {
            map.fitBounds(fieldLayerGroup.getBounds(), { padding: [20, 20] });
        } else {
            emptyState.classList.remove("hidden");
        }

        sel.addEventListener("change", () => {
            if (sel.value === "") resetSelection();
            else renderFieldMeasurements(Number(sel.value), true);
        });

        dateFilter.addEventListener("change", () => {
            if (sel.value !== "") renderFieldMeasurements(Number(sel.value), false);
        });

        sortSelect.addEventListener("change", () => {
            if (sel.value !== "") renderFieldMeasurements(Number(sel.value), false);
        });

        resetSelection();

    } catch (e) {
        console.error(e);
        emptyState.classList.remove("hidden");
        document.getElementById("fields-count").textContent = "Fout bij laden";
    }

    function resetSelection() {
        pointLayerGroup.clearLayers();
        document.getElementById("filterControls").classList.add("opacity-50", "pointer-events-none");
        document.getElementById("dateFilter").value = "";
        document.getElementById("sortSelect").value = "date_desc";

        // reset styles
        fieldsGeoJSON.features.forEach(f => {
            if (f.properties._layer) {
                f.properties._layer.setStyle({ color: "#3b6d11", weight: 2, fillOpacity: 0.1 });
            }
        });

        document.getElementById("field-list").innerHTML = '<li class="px-5 py-6 text-center text-xs text-zinc-500">Kies een veld om de metingen te bekijken.</li>';

        if (fieldLayerGroup.getLayers().length > 0) {
            map.fitBounds(fieldLayerGroup.getBounds(), { padding: [20, 20] });
        }
    }

    function renderFieldMeasurements(id, centerMap = true) {
        const field = fieldsGeoJSON.features.find(f => f.properties._id === id);
        if (!field) return;

        document.getElementById("filterControls").classList.remove("opacity-50", "pointer-events-none");

        // highlight selected field
        if (centerMap) {
            fieldsGeoJSON.features.forEach(f => {
                if (f.properties._layer) {
                    if (f.properties._id === id) {
                        f.properties._layer.setStyle({ color: "#6aa84f", weight: 3, fillOpacity: 0.25 });
                        map.fitBounds(f.properties._layer.getBounds(), { padding: [40, 40] });
                    } else {
                        f.properties._layer.setStyle({ color: "#999", weight: 1, fillOpacity: 0.05 });
                    }
                }
            });
        }

        pointLayerGroup.clearLayers();
        const listHtml = [];

        const filterDate = document.getElementById("dateFilter").value;
        const sortMode = document.getElementById("sortSelect").value;

        // find points inside field
        let fieldPoints = allPoints.filter(pt => turf.booleanPointInPolygon(pt, field));

        // apply date filter
        if (filterDate) {
            fieldPoints = fieldPoints.filter(pt => {
                const ptDate = pt.properties.measured_at ? pt.properties.measured_at.slice(0, 10) : "";
                return ptDate === filterDate;
            });
        }

        // apply sorting
        fieldPoints.sort((a, b) => {
            const propA = a.properties;
            const propB = b.properties;

            if (sortMode === "date_desc") {
                return (propB.measured_at || "").localeCompare(propA.measured_at || "");
            } else if (sortMode === "date_asc") {
                return (propA.measured_at || "").localeCompare(propB.measured_at || "");
            } else if (sortMode === "height_desc") {
                return propB.h - propA.h;
            } else if (sortMode === "height_asc") {
                return propA.h - propB.h;
            }
            return 0;
        });

        fieldPoints.forEach(pt => {
            const r = pt.properties;
            const h = r.h;
            const when = r.measured_at ? r.measured_at.slice(0, 16) : "";

            // add marker to map
            const marker = L.circleMarker([pt.geometry.coordinates[1], pt.geometry.coordinates[0]], {
                radius: 6,
                fillColor: colorFor(h),
                color: "#fff",
                weight: 1.5,
                fillOpacity: 0.9
            }).addTo(pointLayerGroup);

            marker.bindPopup(`
                <div class="font-sans text-xs">
                    <strong class="text-sm">Meting</strong><br>
                    Hoogte: ${h} mm<br>
                    Tijd: ${when}
                </div>
            `);

            r._marker = marker;

            listHtml.push(
                `<li data-point-id="${r.id}" class="px-5 py-3 border-t border-black/5 dark:border-white/10 first:border-t-0 cursor-pointer hover:bg-black/[0.03] dark:hover:bg-white/5 transition-colors">
                    <div class="flex items-center justify-between pointer-events-none">
                        <span class="font-semibold text-sm">Meting</span>
                        <span class="w-2.5 h-2.5 rounded-full" style="background:${colorFor(h)}"></span>
                    </div>
                    <div class="text-[11px] text-zinc-500 mt-0.5 tabular-nums pointer-events-none">Hoogte: ${h} mm (TOF: ${r.tof_mm != null ? r.tof_mm : '—'}, Sonic: ${r.sonic_mm != null ? r.sonic_mm : '—'})</div>
                    <div class="text-[11px] text-zinc-400 pointer-events-none">${when}</div>
                </li>`
            );
        });

        const listEl = document.getElementById("field-list");
        if (listHtml.length > 0) {
            listEl.innerHTML = listHtml.join("");
        } else {
            listEl.innerHTML = '<li class="px-5 py-6 text-center text-xs text-zinc-500">Geen metingen gevonden met deze filters.</li>';
        }
    }

    // interactive highlights
    document.getElementById("field-list").addEventListener("click", (e) => {
        const li = e.target.closest("li[data-point-id]");
        if (!li) return;

        // highlight list item
        document.querySelectorAll("#field-list li").forEach(el => {
            el.classList.remove("bg-brand/10", "dark:bg-brand/20");
        });
        li.classList.add("bg-brand/10", "dark:bg-brand/20");

        const id = Number(li.dataset.pointId);
        const pt = allPoints.find(p => p.properties.id === id);

        if (pt && pt.properties._marker) {
            // reset marker styles
            pointLayerGroup.eachLayer(layer => {
                layer.setStyle({ color: "#fff", weight: 1.5, radius: 6 });
            });

            // emphasize clicked marker
            const marker = pt.properties._marker;
            marker.setStyle({ color: "#1a2e1f", weight: 3, radius: 9 });
            marker.bringToFront();
            marker.openPopup();
        }
    });
})();