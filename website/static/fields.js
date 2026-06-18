(async function () {
    const emptyState = document.getElementById("mapEmpty");
    const map = L.map('mapView', { zoomControl: false }).setView([52.1, 5.2], 7);
    L.control.zoom({ position: 'bottomright' }).addTo(map);

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19, attribution: '© OpenStreetMap'
    }).addTo(map);

    const QUALITY_COLORS = ["#60a526", "#84cc16", "#eab308", "#f97316", "#ef4444"];

    let allPoints = [];
    let fieldsGeoJSON = null;
    let fieldLayerGroup = L.featureGroup().addTo(map);
    let pointLayerGroup = L.featureGroup().addTo(map);
    let qualityMap = {};

    function colorFor(h) {
        if (h <= 70) return "#60a526";
        if (h <= 80) return "#84cc16";
        if (h <= 90) return "#eab308";
        if (h <= 100) return "#f97316";
        return "#ef4444";
    }

    try {
        const fieldsRes = await fetch("/api/fields");
        if (!fieldsRes.ok) throw new Error("Kon velden niet laden");
        const esriData = await fieldsRes.json();

        let summaryData = null;
        try {
            const sumRes = await fetch("/api/summary?days=30");
            summaryData = await sumRes.json();
            if (summaryData && summaryData.fields) {
                summaryData.fields.forEach(f => { qualityMap[String(f.id)] = f.level; });
            }
        } catch (e) {}

        fieldsGeoJSON = {
            type: "FeatureCollection",
            features: (esriData.features || []).map((f, originalIndex) => ({
                type: "Feature",
                properties: { ...(f.attributes || {}), _id: originalIndex },
                geometry: (f.geometry && f.geometry.rings) ? { type: "Polygon", coordinates: f.geometry.rings } : null
            })).filter(f => f.geometry != null)
        };

        const dataRes = await fetch("/api/data");
        const dataJson = await dataRes.json();
        const rows = dataJson.data || [];

        allPoints = rows.filter(r => r.longitude != null && r.latitude != null).map((r, i) => {
            return turf.point([r.longitude, r.latitude], { ...r, id: i, h: r.tof_mm || r.sonic_mm || 0 });
        });

        const sel = document.getElementById("fieldSelect");
        const daysFilter = document.getElementById("daysFilter");
        const sortSelect = document.getElementById("sortSelect");

        const savedDays = localStorage.getItem("daysFilter");
        if (savedDays !== null && daysFilter) {
            daysFilter.value = savedDays;
        }

        sel.innerHTML = '<option value="">Kies een veld…</option>';
        let validFieldsCount = 0;

        fieldsGeoJSON.features.forEach((f) => {
            validFieldsCount++;
            const i = f.properties._id;

            // map attribute lookups to handle inconsistent arcgis field names
            const name = f.properties.Name || f.properties.Naam || f.properties.Field_Name || `Veld ${i + 1}`;

            const opt = document.createElement("option");
            opt.value = i;
            opt.textContent = name;
            sel.appendChild(opt);

            const level = qualityMap[String(i)];
            const fieldColor = (level !== undefined) ? QUALITY_COLORS[level] : "#60a526";
            const layer = L.geoJSON(f, { style: { color: fieldColor, weight: 2, fillColor: fieldColor, fillOpacity: (level !== undefined) ? 0.2 : 0.05 } }).addTo(fieldLayerGroup);
            f.properties._layer = layer;

            // make field clickable on map
            layer.on('click', () => {
                sel.value = i;
                renderFieldMeasurements(i, true);
            });
        });

        document.getElementById("fields-count").textContent = validFieldsCount + " veld" + (validFieldsCount === 1 ? "" : "en");

        if (validFieldsCount > 0) map.fitBounds(fieldLayerGroup.getBounds(), { padding: [20, 20] });
        else emptyState.classList.remove("hidden");

        const urlParams = new URLSearchParams(window.location.search);
        const initialField = urlParams.get('field');

        if (initialField !== null) {
            sel.value = initialField;
            if (sel.value === initialField) {
                renderFieldMeasurements(Number(initialField), true);
            } else {
                resetSelection();
            }
        } else {
            resetSelection();
        }

        sel.addEventListener("change", () => {
            if (sel.value === "") resetSelection();
            else renderFieldMeasurements(Number(sel.value), true);
        });

        daysFilter.addEventListener("change", (e) => {
            localStorage.setItem("daysFilter", e.target.value);
            if (sel.value !== "") renderFieldMeasurements(Number(sel.value), false);
        });
        sortSelect.addEventListener("change", () => { if (sel.value !== "") renderFieldMeasurements(Number(sel.value), false); });

    } catch (e) {
        console.error(e);
        emptyState.classList.remove("hidden");
    }

    function resetSelection() {
        pointLayerGroup.clearLayers();
        const filterBtn = document.getElementById("filterToggleBtn");
        filterBtn.disabled = true;
        filterBtn.classList.add("opacity-50", "pointer-events-none");
        const filterPopup = document.getElementById("filterPopup");
        if (filterPopup) filterPopup.classList.add("hidden");

        const reportLink = document.getElementById("reportLink");
        reportLink.classList.add("opacity-50", "pointer-events-none");
        reportLink.removeAttribute("href");

        fieldsGeoJSON.features.forEach(f => {
            if (f.properties._layer) {
                const level = qualityMap[String(f.properties._id)];
                const fieldColor = (level !== undefined) ? QUALITY_COLORS[level] : "#60a526";
                f.properties._layer.setStyle({ color: fieldColor, weight: 2, fillColor: fieldColor, fillOpacity: (level !== undefined) ? 0.2 : 0.05 });
            }
        });

        document.getElementById("field-list").innerHTML = '<li class="px-5 py-6 text-center text-xs text-zinc-500">Kies een veld om de metingen te bekijken.</li>';
        if (fieldLayerGroup.getLayers().length > 0) map.fitBounds(fieldLayerGroup.getBounds(), { padding: [20, 20] });
    }

    function renderFieldMeasurements(id, centerMap = true) {
        const field = fieldsGeoJSON.features.find(f => f.properties._id === id);
        if (!field) return;

        const filterBtn = document.getElementById("filterToggleBtn");
        filterBtn.disabled = false;
        filterBtn.classList.remove("opacity-50", "pointer-events-none");

        const filterDays = document.getElementById("daysFilter").value;
        const sortMode = document.getElementById("sortSelect").value;
        const reportLink = document.getElementById("reportLink");
        reportLink.classList.remove("opacity-50", "pointer-events-none");
        reportLink.href = `/report?field=${id}&days=${filterDays || '30'}`;

        if (centerMap) {
            fieldsGeoJSON.features.forEach(f => {
                if (f.properties._layer) {
                    if (f.properties._id === id) {
                        f.properties._layer.setStyle({ color: "#7bc53b", weight: 3, fillColor: "#7bc53b", fillOpacity: 0.25 });
                        map.fitBounds(f.properties._layer.getBounds(), { padding: [40, 40] });
                    } else {
                        f.properties._layer.setStyle({ color: "#999", weight: 1, fillColor: "#999", fillOpacity: 0.05 });
                    }
                }
            });
        }

        // clear existing markers before applying new filters
        pointLayerGroup.clearLayers();
        const listHtml = [];

        // client-side spatial join to assign scattered sensor points to field polygons
        let fieldPoints = allPoints.filter(pt => turf.booleanPointInPolygon(pt, field));

        if (filterDays !== "") {
            const daysNum = parseInt(filterDays, 10);
            const cutoffDate = new Date();
            cutoffDate.setDate(cutoffDate.getDate() - daysNum);

            fieldPoints = fieldPoints.filter(pt => {
                if (!pt.properties.measured_at) return false;
                const ptDate = new Date(pt.properties.measured_at);
                return ptDate >= cutoffDate;
            });
        }

        fieldPoints.sort((a, b) => {
            if (sortMode === "date_desc") return (b.properties.measured_at || "").localeCompare(a.properties.measured_at || "");
            if (sortMode === "date_asc") return (a.properties.measured_at || "").localeCompare(b.properties.measured_at || "");
            if (sortMode === "height_desc") return b.properties.h - a.properties.h;
            if (sortMode === "height_asc") return a.properties.h - b.properties.h;
            return 0;
        });

        const sortLabels = {
            "date_desc": "Nieuwste",
            "date_asc": "Oudste",
            "height_desc": "Hoogste",
            "height_asc": "Laagste"
        };
        const daysLabel = filterDays ? `${filterDays} dagen` : "Alle";
        const sortLabel = sortLabels[sortMode] || "Nieuwste";

        listHtml.push(
            `<li class="px-4 py-2 bg-black/[0.02] dark:bg-white/[0.02] border-b border-black/5 dark:border-white/10 text-[9px] font-bold uppercase tracking-wider text-zinc-500 sticky top-0 z-10 backdrop-blur-md flex items-center justify-between">
                <span>${daysLabel} &middot; ${sortLabel}</span>
                <span class="text-zinc-400">${fieldPoints.length} resultaten</span>
            </li>`
        );

        fieldPoints.forEach(pt => {
            const r = pt.properties;
            const h = r.h;
            const when = r.measured_at ? r.measured_at.slice(0, 16) : "";

            const marker = L.circleMarker([pt.geometry.coordinates[1], pt.geometry.coordinates[0]], {
                radius: 6, fillColor: colorFor(h), color: "#fff", weight: 1.5, fillOpacity: 0.9
            }).addTo(pointLayerGroup);

            marker.bindPopup(`<div class="font-sans text-xs"><strong class="text-sm">Meting</strong><br>Hoogte: ${h} mm<br>Tijd: ${when}</div>`);
            r._marker = marker;

            marker.on('click', () => {
                document.querySelectorAll("#field-list li[data-point-id]").forEach(el => el.classList.remove("bg-black/5", "dark:bg-white/10"));
                const li = document.querySelector(`#field-list li[data-point-id="${r.id}"]`);
                if (li) {
                    li.classList.add("bg-black/5", "dark:bg-white/10");
                    li.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
                }

                pointLayerGroup.eachLayer(layer => layer.setStyle({ color: "#fff", weight: 1.5, radius: 6 }));
                marker.setStyle({ color: "#1a2e1f", weight: 3, radius: 9 });
                marker.bringToFront();
            });

            listHtml.push(
                `<li data-point-id="${r.id}" class="px-4 py-2.5 border-b border-black/5 dark:border-white/10 last:border-b-0 cursor-pointer hover:bg-black/[0.03] dark:hover:bg-white/5 transition-colors flex items-center justify-between gap-3">
                    <div class="flex flex-col min-w-0 pointer-events-none">
                        <div class="font-semibold text-sm truncate">Meting &middot; <span class="font-normal text-zinc-500">${h} mm</span></div>
                        <div class="text-[11px] text-zinc-400 tabular-nums">${when}</div>
                    </div>
                    <span class="w-2.5 h-2.5 rounded-full shrink-0 pointer-events-none" style="background:${colorFor(h)}"></span>
                </li>`
            );
        });

        const listEl = document.getElementById("field-list");
        if (fieldPoints.length > 0) {
            listEl.innerHTML = listHtml.join("");
        } else {
            listHtml.push('<li class="px-5 py-6 text-center text-xs text-zinc-500">Geen metingen gevonden in deze periode.</li>');
            listEl.innerHTML = listHtml.join("");
        }

        if (window.expandMobileList) {
            window.expandMobileList();
        }
    }

    document.getElementById("field-list").addEventListener("click", (e) => {
        const li = e.target.closest("li[data-point-id]");
        if (!li) return;

        document.querySelectorAll("#field-list li[data-point-id]").forEach(el => el.classList.remove("bg-black/5", "dark:bg-white/10"));
        li.classList.add("bg-black/5", "dark:bg-white/10");

        const id = Number(li.dataset.pointId);
        const pt = allPoints.find(p => p.properties.id === id);

        if (pt && pt.properties._marker) {
            pointLayerGroup.eachLayer(layer => layer.setStyle({ color: "#fff", weight: 1.5, radius: 6 }));
            const marker = pt.properties._marker;
            // bring selected marker to foreground and increase size for visibility
            marker.setStyle({ color: "#1a2e1f", weight: 3, radius: 9 });
            marker.bringToFront(); marker.openPopup();
        }
    });
})();