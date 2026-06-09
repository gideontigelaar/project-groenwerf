let reportData = null;
let mainChart = null;
let dashDonutChart = null;
let selectedFieldId = null;
let miniMap = null;
let fieldLayerGroup = null;

async function loadData(days = "30") {
    try {
        const res = await fetch(`/api/summary?days=${days}`);
        reportData = await res.json();
        document.getElementById("kpi-source").textContent = reportData.source === "arcgis" ? "ArcGIS" : "Geen bron";
        renderKPIs();
        renderTable();
        renderCharts();
        renderActivity();
        initMiniMap();
    } catch (e) {
        document.getElementById("fields-body").innerHTML = '<tr><td colspan="5" class="px-4 py-6 text-center text-[#ef4444] text-sm">Kon data niet laden</td></tr>';
    }
}

document.getElementById("daysFilter").addEventListener("change", (e) => {
    selectedFieldId = null;
    document.getElementById("chart-reset-wrapper").classList.add("hidden");
    loadData(e.target.value);
});

function gradeStyle(level) {
    const th = window.chartTheme();
    const colors = [th.aplus, th.a, th.b, th.c, th.d];
    return { color: colors[level] };
}

function renderKPIs() {
    if (!reportData) return;

    const activeBadge = document.getElementById("table-active-fields");
    if(activeBadge) {
        activeBadge.textContent = `${reportData.fields.length} Actieve Velden`;
    }

    document.getElementById("kpi-count").textContent = reportData.total;
    document.getElementById("kpi-avg").innerHTML = `${reportData.avg} <span class="text-sm font-normal text-zinc-500">mm</span>`;

    document.getElementById("kpi-mow").textContent = reportData.counts[4];
    document.getElementById("kpi-mow-badge").textContent = `${reportData.counts[4]} meting${reportData.counts[4] === 1 ? "" : "en"}`;
    document.getElementById("kpi-time").textContent = reportData.latest;

    const avgBadge = document.getElementById("kpi-avg-badge");
    if (reportData.avg >= 90) {
        avgBadge.textContent = "Boven streefwaarde";
        avgBadge.className = "badge rounded-pill text-bg-warning";
    } else {
        avgBadge.textContent = "Normaal";
        avgBadge.className = "badge rounded-pill text-bg-success";
    }
}

function renderTable() {
    if (!reportData) return;
    const tbody = document.getElementById("fields-body");
    const mobileList = document.getElementById("fields-body-mobile");

    if (!reportData.fields.length) {
        if (tbody) tbody.innerHTML = '<tr><td colspan="5" class="px-4 py-6 text-center text-zinc-500 text-sm">Geen velden of metingen</td></tr>';
        if (mobileList) mobileList.innerHTML = '<li class="px-4 py-6 text-center text-zinc-500 text-sm">Geen velden of metingen</li>';
        return;
    }

    if (tbody) {
        tbody.innerHTML = reportData.fields.map(f => {
            const style = gradeStyle(f.level);
            const isActive = selectedFieldId === String(f.id) ? "bg-black/5 dark:bg-white/10" : "";
            return `<tr data-field-id="${f.id}" class="border-t border-black/5 dark:border-white/10 hover:bg-black/[0.03] dark:hover:bg-white/[0.03] transition-colors cursor-pointer ${isActive}">
                <td class="px-4 py-2.5 font-semibold">${f.name}</td>
                <td class="px-4 py-2.5 text-[11px] text-zinc-500">${f.latest ? f.latest.slice(0, 16) : "—"}</td>
                <td class="px-4 py-2.5 min-w-[120px]">
                    <div class="flex items-center gap-2">
                        <div class="flex-1 h-1.5 rounded-full bg-black/10 dark:bg-white/10 overflow-hidden">
                            <div class="h-full rounded-full" style="width:${f.bar_pct}%;background-color:${style.color}"></div>
                        </div>
                        <div class="text-[11px] text-zinc-500 tabular-nums w-10">${f.avg} mm</div>
                    </div>
                </td>
                <td class="px-4 py-2.5 text-center">
                    <span class="badge rounded-pill w-10" style="background-color:${style.color}; color:#fff; border:none;">${f.label}</span>
                </td>
                <td class="px-4 py-2.5 text-right">
                    <a href="/fields?field=${f.id}" class="map-link inline-flex p-1.5 rounded-lg bg-black/5 dark:bg-white/10 hover:bg-brand hover:text-white transition-colors !no-underline !text-zinc-600 dark:!text-zinc-400 hover:!text-white" title="Bekijk op kaart">
                        <i class="ph-fill ph-map-pin text-[16px]"></i>
                    </a>
                </td>
            </tr>`;
        }).join("");
    }

    if (mobileList) {
        mobileList.innerHTML = reportData.fields.map(f => {
            const style = gradeStyle(f.level);
            const isActive = selectedFieldId === String(f.id) ? "bg-black/5 dark:bg-white/10" : "";
            return `<li data-field-id="${f.id}" class="flex items-center gap-3 px-4 py-2.5 border-t border-black/5 dark:border-white/10 cursor-pointer hover:bg-black/[0.03] dark:hover:bg-white/[0.03] transition-colors ${isActive}">
                <span class="badge rounded-pill shrink-0 w-9 text-center" style="background-color:${style.color}; color:#fff; border:none;">${f.label}</span>
                <div class="flex-1 min-w-0 flex flex-col">
                    <div class="font-semibold text-sm truncate w-full">${f.name}</div>
                    <div class="flex items-center gap-2 mt-1 w-full">
                        <div class="flex-1 h-1 rounded-full bg-black/10 dark:bg-white/10 overflow-hidden min-w-0">
                            <div class="h-full rounded-full" style="width:${f.bar_pct}%;background-color:${style.color}"></div>
                        </div>
                        <span class="text-[11px] text-zinc-500 tabular-nums shrink-0">${f.avg} mm</span>
                    </div>
                </div>
                <a href="/fields?field=${f.id}" class="map-link shrink-0 inline-flex p-1.5 rounded-lg bg-black/5 dark:bg-white/10 hover:bg-brand hover:text-white transition-colors !no-underline !text-zinc-600 dark:!text-zinc-400 hover:!text-white" title="Bekijk op kaart">
                    <i class="ph-fill ph-map-pin text-[16px]"></i>
                </a>
            </li>`;
        }).join("");
    }
}

// handle dynamic element clicks
function _handleFieldClick(e) {
    const btn = e.target.closest(".map-link");
    if (btn) return;
    const el = e.target.closest("[data-field-id]");
    if (!el) return;

    selectedFieldId = el.dataset.fieldId;
    const f = reportData.fields.find(x => String(x.id) === String(selectedFieldId));
    document.getElementById("chart-selected-text").textContent = `Historie: ${f.name}`;
    document.getElementById("chart-reset-wrapper").classList.remove("hidden");

    renderTable();
    renderCharts();
    highlightMiniMap(true);
}

document.getElementById("fields-body").addEventListener("click", _handleFieldClick);
document.getElementById("fields-body-mobile").addEventListener("click", _handleFieldClick);

document.getElementById("chart-reset").addEventListener("click", () => {
    selectedFieldId = null;
    renderTable();
    renderCharts();
    document.getElementById("chart-reset-wrapper").classList.add("hidden");
    highlightMiniMap(false);
});

function renderCharts() {
    if (!reportData) return;
    const th = window.chartTheme();
    const colors = [th.aplus, th.a, th.b, th.c, th.d];

    const ctx = document.getElementById("mainChart");
    // destroy previous instances to prevent issues
    if (mainChart) mainChart.destroy();

    if (selectedFieldId !== null) {
        const f = reportData.fields.find(x => String(x.id) === String(selectedFieldId));
        document.getElementById("chart-title").textContent = `Maaibeurten: ${f.name}`;
        document.getElementById("chart-sub").innerHTML = "Verloop van de metingen / maaibeurten";

        mainChart = new Chart(ctx, {
            type: "line",
            data: {
                labels: f.history.map(r => r.date.slice(5)),
                datasets: [{
                    label: "Grashoogte", data: f.history.map(r => r.h),
                    borderColor: th.brand, backgroundColor: "rgba(96,165,38,0.15)", fill: true, tension: 0.3, pointRadius: 4, pointBackgroundColor: f.history.map(r => r.is_mow ? th.brand : th.a)
                }]
            },
            options: {
                responsive: true, maintainAspectRatio: false,
                plugins: { tooltip: { callbacks: { label: c => `Grashoogte: ${c.parsed.y} mm` } }, legend: { display: false } },
                scales: { x: { ticks: { color: th.text } }, y: { grid: { color: th.grid }, ticks: { color: th.text, callback: v => v + " mm" }, beginAtZero: true } }
            }
        });
    } else {
        document.getElementById("chart-title").textContent = "Grashoogte per Veld";
        document.getElementById("chart-sub").innerHTML = "Gemiddelde hoogte per veld <i>(Klik op een veld in de lijst voor details)</i>";

        mainChart = new Chart(ctx, {
            type: "bar",
            data: {
                labels: reportData.fields.map(f => f.name),
                datasets: [{
                    label: "Grashoogte", data: reportData.fields.map(f => f.avg),
                    backgroundColor: reportData.fields.map(f => colors[f.level]), borderRadius: 4, maxBarThickness: 32
                }]
            },
            options: {
                responsive: true, maintainAspectRatio: false,
                plugins: { legend: { display: false }, tooltip: { callbacks: { label: c => `Grashoogte: ${c.parsed.y} mm` } } },
                scales: { x: { grid: { display: false }, ticks: { color: th.text } }, y: { grid: { color: th.grid }, ticks: { color: th.text, callback: v => v + " mm" }, beginAtZero: true } }
            }
        });
    }

    renderDonut();
}

function renderDonut() {
    if (!reportData) return;
    const th = window.chartTheme();
    const colors = [th.aplus, th.a, th.b, th.c, th.d];
    const counts = reportData.counts;
    const total = counts.reduce((a, b) => a + b, 0);

    const ctx2 = document.getElementById("dashDonutChart");
    if (!ctx2) return;
    if (dashDonutChart) dashDonutChart.destroy();
    dashDonutChart = new Chart(ctx2, {
        type: "doughnut",
        data: { labels: ["A+", "A", "B", "C", "D"], datasets: [{ data: counts, backgroundColor: colors, borderWidth: 0 }] },
        options: { responsive: true, maintainAspectRatio: false, cutout: "72%", plugins: { legend: { display: false } } }
    });

    const pct = total ? Math.round(((counts[0] + counts[1]) / total) * 100) : 0;
    const pctEl = document.getElementById("dash-donut-pct");
    if (pctEl) pctEl.textContent = pct + "%";

    counts.forEach((c, i) => {
        const el = document.getElementById(`dash-count-${i}`);
        if (el) el.textContent = c;
    });
}

function renderActivity() {
    const list = document.getElementById("activity-list");
    if (!reportData || !reportData.fields.length) {
        list.innerHTML = '<li class="px-4 py-3 text-xs text-zinc-500">Geen activiteit</li>'; return;
    }
    const recent = reportData.fields.filter(f => f.level >= 3);
    if (!recent.length) {
        list.innerHTML = '<li class="px-4 py-3 text-xs text-zinc-500">Geen velden met kwaliteit C of D. Alles in orde!</li>'; return;
    }

    const th = window.chartTheme();
    const colors = [th.aplus, th.a, th.b, th.c, th.d];

    list.innerHTML = recent.map(f => `
        <li class="flex items-start gap-3 px-4 py-2.5 border-t border-black/5 dark:border-white/10 first:border-t-0">
            <span class="w-2 h-2 rounded-full mt-1.5 shrink-0" style="background-color:${colors[f.level]}"></span>
            <div><div class="text-xs font-semibold">${f.name} — <span class="tabular-nums">${f.avg} mm</span> (${f.label})</div>
            <div class="text-[11px] text-zinc-500 mt-0.5">Actie: ${f.action}</div></div>
        </li>`).join("");
}

const _miniMapLayers = {};

async function initMiniMap() {
    if (miniMap) return;
    miniMap = L.map('miniMapView', { zoomControl: false, attributionControl: false }).setView([52.1, 5.2], 7);
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', { maxZoom: 19 }).addTo(miniMap);
    fieldLayerGroup = L.featureGroup().addTo(miniMap);

    try {
        const res = await fetch("/api/fields");
        const json = await res.json();

        const th = window.chartTheme();
        const colors = [th.aplus, th.a, th.b, th.c, th.d];

        json.features.forEach((f, i) => {
            if (!f.geometry) return;
            // map arcgis ring arrays to standard geojson for leaflet rendering
            const geoJsonFeat = { type: "Feature", properties: { _id: i }, geometry: { type: "Polygon", coordinates: f.geometry.rings } };

            const fd = reportData ? reportData.fields.find(x => String(x.id) === String(i)) : null;
            const color = fd ? colors[fd.level] : th.brand;
            const fillOpacity = fd ? 0.2 : 0.1;

            const layer = L.geoJSON(geoJsonFeat, { style: { color, weight: 2, fillOpacity } }).addTo(fieldLayerGroup);
            _miniMapLayers[String(i)] = layer;

            layer.on("click", () => {
                selectedFieldId = String(i);
                document.getElementById("chart-reset-wrapper").classList.remove("hidden");
                renderTable(); renderCharts(); highlightMiniMap(true);
                const target = document.querySelector(`[data-field-id="${i}"]`);
                target?.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
            });
        });
        if (fieldLayerGroup.getLayers().length > 0) miniMap.fitBounds(fieldLayerGroup.getBounds(), { padding: [10, 10] });
    } catch (e) { }
}

function highlightMiniMap(zoomToSelected = false) {
    if (!fieldLayerGroup || !reportData) return;

    const th = window.chartTheme();
    const colors = [th.aplus, th.a, th.b, th.c, th.d];

    Object.entries(_miniMapLayers).forEach(([fId, layer]) => {
        const fd = reportData.fields.find(x => String(x.id) === fId);
        const color = fd ? colors[fd.level] : "#999";

        // dim unselected map layers and highlight active selection
        if (selectedFieldId === null) {
            layer.setStyle({ color, weight: 2, fillOpacity: 0.2 });
        } else if (fId === selectedFieldId) {
            layer.setStyle({ color, weight: 3, fillOpacity: 0.4 });
            if (zoomToSelected) {
                miniMap.fitBounds(layer.getBounds(), { padding: [30, 30], maxZoom: 16 });
            }
        } else {
            layer.setStyle({ color: "#999", weight: 1, fillOpacity: 0.05 });
        }
    });

    if (selectedFieldId === null) {
        miniMap.fitBounds(fieldLayerGroup.getBounds(), { padding: [10, 10] });
    }
}

const syncBtn = document.getElementById("syncBtn");
const syncDesktopText = document.getElementById("syncDesktopText");
const syncMobileText = document.getElementById("syncMobileText");

if (syncBtn) {
    syncBtn.addEventListener("click", async () => {
        syncBtn.disabled = true;
        syncDesktopText.textContent = "Bezig met synchroniseren...";
        syncMobileText.textContent = "Bezig...";
        try {
            await fetch("/api/sync", { method: "POST" });
            await loadData(document.getElementById("daysFilter").value);
        } catch (e) {}
        syncDesktopText.textContent = "Synchroniseren";
        syncMobileText.textContent = "Sync";
        syncBtn.disabled = false;
    });
}

window.addEventListener("themechange", () => { if (reportData) { renderCharts(); } });
loadData();
// polling loop for real-time background sync
setInterval(() => loadData(document.getElementById("daysFilter").value), 30000);