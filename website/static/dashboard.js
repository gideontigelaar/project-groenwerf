// manage dashboard data loading, ui sync, and rendering
const MOW = 400;
const WATCH = 80;

function statusOf(tof, sonic) {
    const h = (tof || sonic || 0);
    if (h >= MOW) return { label: "Maaien", level: 2, color: "#ef4444", badge: "text-bg-danger" };
    if (h >= WATCH) return { label: "Let op", level: 1, color: "#f59e0b", badge: "text-bg-warning" };
    return { label: "Goed", level: 0, color: "#6aa84f", badge: "text-bg-success" };
}

let allRows = [];
let currentFilter = "all";
let donutChart = null;
let trendChart = null;

async function loadData() {
    try {
        const res = await fetch("/api/data");
        const json = await res.json();
        allRows = (json.data || []);
        document.getElementById("kpi-source").textContent = json.meta && json.meta.source === "arcgis" ? "ArcGIS" : "Geen bron";
        renderKPIs();
        renderTable(currentFilter);
        renderCharts();
        renderActivity();
    } catch (e) {
        document.getElementById("fields-body").innerHTML = '<tr><td colspan="5" class="px-5 py-8 text-center text-[#ef4444] text-sm">Kon data niet laden</td></tr>';
    }
}

function heightOf(r) {
    return (r.tof_mm || r.sonic_mm || 0);
}

function renderKPIs() {
    const count = allRows.length;
    const heights = allRows.map(heightOf);
    const avg = heights.length ? Math.round(heights.reduce((a, b) => a + b, 0) / heights.length) : 0;
    const mow = allRows.filter(r => statusOf(r.tof_mm, r.sonic_mm).level >= 2).length;
    const latest = allRows[0]?.measured_at || "—";

    document.getElementById("kpi-count").textContent = count;
    document.getElementById("kpi-avg").innerHTML = `${avg} <span class="text-sm font-normal text-zinc-500">mm</span>`;
    document.getElementById("kpi-mow").textContent = mow;
    document.getElementById("kpi-mow-badge").textContent = `${mow} meting${mow === 1 ? "" : "en"}`;
    document.getElementById("kpi-time").textContent = latest !== "—" ? latest.slice(11, 16) : "—";

    const avgBadge = document.getElementById("kpi-avg-badge");
    if (avg >= 200) {
        avgBadge.textContent = "Boven streefwaarde";
        avgBadge.className = "badge rounded-pill text-bg-warning";
    } else {
        avgBadge.textContent = "Normaal";
        avgBadge.className = "badge rounded-pill text-bg-success";
    }
}

function renderTable(filter) {
    const rows = allRows.filter(r => {
        const lvl = statusOf(r.tof_mm, r.sonic_mm).level;
        if (filter === "mow") return lvl >= 2;
        if (filter === "ok") return lvl === 0;
        return true;
    }).slice(0, 25);

    const tbody = document.getElementById("fields-body");
    if (!rows.length) {
        tbody.innerHTML = '<tr><td colspan="5" class="px-5 py-8 text-center text-zinc-500 text-sm">Geen data</td></tr>';
        return;
    }
    tbody.innerHTML = rows.map(r => {
        const tof = r.tof_mm ?? null;
        const sonic = r.sonic_mm ?? null;
        const h = heightOf(r);
        const s = statusOf(tof, sonic);
        const t = r.measured_at ? r.measured_at.slice(11, 16) : "—";
        const d = r.measured_at ? r.measured_at.slice(0, 10) : "—";
        const pct = Math.min(100, Math.round(h / 8));
        return `<tr class="border-t border-black/5 dark:border-white/10 hover:bg-black/[0.025] dark:hover:bg-white/5 transition-colors">
            <td class="px-5 py-3"><div class="font-semibold">${t}</div><div class="text-[11px] text-zinc-500">${d}</div></td>
            <td class="px-5 py-3 tabular-nums">${tof != null ? tof : "—"}</td>
            <td class="px-5 py-3 tabular-nums">${sonic != null ? sonic : "—"}</td>
            <td class="px-5 py-3">
                <div class="w-24 h-1.5 rounded-full bg-black/10 dark:bg-white/10 overflow-hidden">
                    <div class="h-full rounded-full" style="width:${pct}%;background:${s.color}"></div>
                </div>
                <div class="text-[11px] text-zinc-500 mt-1 tabular-nums">${h} mm</div>
            </td>
            <td class="px-5 py-3"><span class="badge rounded-pill ${s.badge}">${s.label}</span></td>
        </tr>`;
    }).join("");
}

document.getElementById("filter-pills").addEventListener("click", (e) => {
    const btn = e.target.closest("button[data-filter]");
    if (!btn) return;
    currentFilter = btn.dataset.filter;
    document.querySelectorAll("#filter-pills .pill").forEach(p => {
        p.className = "pill px-3 py-1 rounded-full text-xs font-semibold border border-black/10 dark:border-white/15 text-zinc-500 dark:text-zinc-400 transition-colors";
    });
    btn.className = "pill px-3 py-1 rounded-full text-xs font-semibold border border-brand bg-brand/10 text-brand transition-colors";
    renderTable(currentFilter);
});

function renderCharts() {
    const counts = [0, 0, 0];
    allRows.forEach(r => counts[statusOf(r.tof_mm, r.sonic_mm).level]++);
    const total = allRows.length || 1;
    document.getElementById("donut-pct").textContent = Math.round((counts[0] / total) * 100) + "%";

    const th = window.chartTheme();

    if (donutChart) donutChart.destroy();
    donutChart = new Chart(document.getElementById("donutChart"), {
        type: "doughnut",
        data: {
            labels: ["Goed", "Let op", "Maaien"],
            datasets: [{ data: counts, backgroundColor: [th.ok, th.warn, th.mow], borderWidth: 0, hoverOffset: 6 }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            cutout: "72%",
            plugins: {
                legend: { display: false },
                tooltip: { callbacks: { label: c => `${c.label}: ${c.parsed}` } }
            }
        }
    });

    const series = allRows.slice(0, 40).reverse();
    if (trendChart) trendChart.destroy();
    trendChart = new Chart(document.getElementById("trendChart"), {
        type: "line",
        data: {
            labels: series.map(r => (r.measured_at || "").slice(11, 19)),
            datasets: [{
                data: series.map(heightOf),
                borderColor: th.brand,
                borderWidth: 2,
                tension: 0.35,
                pointRadius: 0,
                fill: true,
                backgroundColor: "rgba(106,168,79,0.12)",
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { display: false },
                tooltip: { callbacks: { label: c => `${c.parsed.y} mm` } }
            },
            scales: {
                x: { grid: { color: th.grid }, ticks: { color: th.text, maxTicksLimit: 8, font: { size: 10 } } },
                y: { grid: { color: th.grid }, ticks: { color: th.text, font: { size: 11 }, callback: v => v + " mm" }, beginAtZero: true }
            }
        }
    });
}

function renderActivity() {
    const list = document.getElementById("activity-list");
    const recent = allRows.slice(0, 6);
    if (!recent.length) {
        list.innerHTML = '<li class="px-5 py-3 text-xs text-zinc-500">Geen activiteit</li>';
        return;
    }
    list.innerHTML = recent.map(r => {
        const h = heightOf(r);
        const s = statusOf(r.tof_mm, r.sonic_mm);
        const t = r.measured_at ? r.measured_at.slice(11, 16) : "—";
        return `<li class="flex items-start gap-3 px-5 py-3 border-t border-black/5 dark:border-white/10 first:border-t-0">
            <span class="w-2 h-2 rounded-full mt-1.5 shrink-0" style="background:${s.color}"></span>
            <div><div class="text-xs">Meting ontvangen — <span class="font-semibold tabular-nums">${h} mm</span> (${s.label})</div>
            <div class="text-[11px] text-zinc-500 mt-0.5">${t}</div></div>
        </li>`;
    }).join("");
}

// manual sync button handling
const syncBtn = document.getElementById("syncBtn");
if (syncBtn) {
    syncBtn.addEventListener("click", async () => {
        syncBtn.disabled = true;
        syncBtn.textContent = "Syncing...";
        try {
            await fetch("/api/sync", { method: "POST" });
            await loadData();
        } catch (e) {
            console.error("sync failed", e);
        }
        syncBtn.textContent = "Sync nu";
        syncBtn.disabled = false;
    });
}

window.addEventListener("themechange", () => {
    if (allRows.length) renderCharts();
});

loadData();
setInterval(loadData, 30000);