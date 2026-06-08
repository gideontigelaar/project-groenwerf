// load locations fully from backend proxy endpoint to fix client arcgis sync/auth limits
(function () {
    const emptyState = document.getElementById("mapEmpty");

    if (typeof require === "undefined") {
        emptyState.classList.remove("hidden");
        document.getElementById("fields-count").textContent = "Fout bij laden";
        return;
    }

    require([
        "esri/Map",
        "esri/views/MapView",
        "esri/Graphic",
        "esri/layers/GraphicsLayer"
    ], function (Map, MapView, Graphic, GraphicsLayer) {

        const map = new Map({ basemap: "topo-vector" });
        const graphicsLayer = new GraphicsLayer();
        map.add(graphicsLayer);
        const view = new MapView({ container: "mapView", map: map, zoom: 11 });

        let graphicsById = {};

        function colorFor(h) {
            return h >= 400 ? "#ef4444" : h >= 80 ? "#f59e0b" : "#6aa84f";
        }

        function select(oid) {
            const g = graphicsById[oid];
            if (!g) return;
            view.goTo({ target: g.geometry, zoom: 16 }).catch(() => {});
            document.getElementById("fieldSelect").value = String(oid);
            document.querySelectorAll("#field-list li").forEach(li => {
                li.classList.toggle("bg-brand/10", li.dataset.oid === String(oid));
            });
        }

        view.when(function () {
            fetch("/api/data")
                .then(r => r.json())
                .then(json => {
                    const rows = json.data || [];
                    const validRows = rows.filter(r => r.longitude != null && r.latitude != null);

                    document.getElementById("fields-count").textContent = validRows.length + " veld" + (validRows.length === 1 ? "" : "en");

                    const sel = document.getElementById("fieldSelect");
                    const list = document.getElementById("field-list");
                    const listHtml = [];

                    validRows.forEach((r, i) => {
                        const oid = i;
                        const h = r.tof_mm || r.sonic_mm || 0;
                        const when = r.measured_at ? r.measured_at.slice(0, 16) : "";
                        const name = "Veld " + (i + 1);

                        const point = { type: "point", longitude: r.longitude, latitude: r.latitude };
                        const marker = {
                            type: "simple-marker",
                            color: colorFor(h),
                            outline: { color: "#ffffff", width: 1 }
                        };

                        const graphic = new Graphic({
                            geometry: point,
                            symbol: marker,
                            attributes: { oid: oid, name: name, h: h, when: when }
                        });
                        graphicsLayer.add(graphic);
                        graphicsById[oid] = graphic;

                        if (i === 0) {
                            view.center = [r.longitude, r.latitude];
                        }

                        const opt = document.createElement("option");
                        opt.value = String(oid);
                        opt.textContent = `${name} — ${h} mm`;
                        sel.appendChild(opt);

                        listHtml.push(
                            `<li data-oid="${oid}" class="px-5 py-3 border-t border-black/5 dark:border-white/10 first:border-t-0 cursor-pointer hover:bg-black/[0.03] dark:hover:bg-white/5 transition-colors">
                                <div class="flex items-center justify-between">
                                    <span class="font-semibold text-sm">${name}</span>
                                    <span class="w-2.5 h-2.5 rounded-full" style="background:${colorFor(h)}"></span>
                                </div>
                                <div class="text-[11px] text-zinc-500 mt-0.5 tabular-nums">${h} mm</div>
                                <div class="text-[11px] text-zinc-400">${when}</div>
                            </li>`
                        );
                    });

                    list.innerHTML = listHtml.join("") || '<li class="px-5 py-6 text-center text-xs text-zinc-500">Geen velden gevonden</li>';

                    if (!validRows.length) {
                        emptyState.classList.remove("hidden");
                    }

                    list.addEventListener("click", e => {
                        const li = e.target.closest("li[data-oid]");
                        if (li) select(li.dataset.oid);
                    });
                    sel.addEventListener("change", () => {
                        if (sel.value) select(sel.value);
                    });
                })
                .catch(err => {
                    console.error(err);
                    document.getElementById("fields-count").textContent = "Laden mislukt";
                });
        });
    });
})();