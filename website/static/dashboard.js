// manage dashboard data loading and rendering
let reportData = null;
let currentFilter = "all";
let donutChart = null;
let barChart = null;

async function loadData() {
    try {
        const res = await fetch("/api/summary");
        const json = await res.json();
        reportData = json;
        document.getElementById("kpi-source").textContent = json.source === "arcgis" ? "ArcGIS" : "Geen bron";
        renderKPIs();
        renderTable(currentFilter);
        renderCharts();
        renderActivity();
    } catch (e) {
        document.getElementById("fields-body").innerHTML = '<tr><td colspan="5" class="px-5 py-8 text-center text-[#ef4444] text-sm">Kon data niet laden</td></tr>';
    }
}

function renderKPIs() {
    if (!reportData) return;

    document.getElementById("kpi-count").textContent = reportData.total;
    document.getElementById("kpi-avg").innerHTML = `${reportData.avg} <span class="text-sm font-normal text-zinc-500">mm</span>`;
    document.getElementById("kpi-mow").textContent = reportData.counts_mow;
    document.getElementById("kpi-mow-badge").textContent = `${reportData.counts_mow} meting${reportData.counts_mow === 1 ? "" : "en"}`;
    document.getElementById("kpi-time").textContent = reportData.latest !== "—" ? reportData.latest.slice(11, 16) : "—";

    const avgBadge = document.getElementById("kpi-avg-badge");
    if (reportData.avg >= 200) {
        avgBadge.textContent = "Boven streefwaarde";
        avgBadge.className = "badge rounded-pill text-bg-warning";
    } else {
        avgBadge.textContent = "Normaal";
        avgBadge.className = "badge rounded-pill text-bg-success";
    }
}

function renderTable(filter) {
    if (!reportData) return;

    const rows = reportData.fields.filter(f => {
        if (filter === "mow") return f.level >= 2;
        if (filter === "ok") return f.level === 0;
        return true;
    });

    const tbody = document.getElementById("fields-body");
    if (!rows.length) {
        tbody.innerHTML = '<tr><td colspan="5" class="px-5 py-8 text-center text-zinc-500 text-sm">Geen velden</td></tr>';
        return;
    }

    tbody.innerHTML = rows.map(f => {
        const color = f.level === 2 ? "#ef4444" : f.level === 1 ? "#f59e0b" : "#6aa84f";
        const badge = f.level === 2 ? "text-bg-danger" : f.level === 1 ? "text-bg-warning" : "text-bg-success";
        return `<tr class="border-t border-black/5 dark:border-white/10 hover:bg-black/[0.025] dark:hover:bg-white/5 transition-colors">
            <td class="px-5 py-3 font-semibold">${f.name}</td>
            <td class="px-5 py-3 text-[11px] text-zinc-500">${f.latest ? f.latest.slice(0, 16) : "—"}</td>
            <td class="px-5 py-3 tabular-nums">${f.total}</td>
            <td class="px-5 py-3">
                <div class="w-24 h-1.5 rounded-full bg-black/10 dark:bg-white/10 overflow-hidden">
                    <div class="h-full rounded-full" style="width:${f.bar_pct}%;background:${color}"></div>
                </div>
                <div class="text-[11px] text-zinc-500 mt-1 tabular-nums">${f.avg} mm</div>
            </td>
            <td class="px-5 py-3"><span class="badge rounded-pill ${badge}">${f.label}</span></td>
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
    if (!reportData) return;
    const th = window.chartTheme();

    // donut
    if (donutChart) donutChart.destroy();
    donutChart = new Chart(document.getElementById("donutChart"), {
        type: "doughnut",
        data: {
            labels: ["Goed", "Let op", "Maaien"],
            datasets: [{ data: [reportData.counts_ok, reportData.counts_warn, reportData.counts_mow], backgroundColor: [th.ok, th.warn, th.mow], borderWidth: 0, hoverOffset: 6 }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            cutout: "72%",
            plugins: {
                legend: { display: false }
            }
        }
    });

    document.getElementById("donut-pct").textContent = reportData.pct_ok + "%";

    // bar chart showing avg height per field
    if (barChart) barChart.destroy();
    barChart = new Chart(document.getElementById("barChart"), {
        type: "bar",
        data: {
            labels: reportData.fields.map(f => f.name),
            datasets: [{
                data: reportData.fields.map(f => f.avg),
                backgroundColor: reportData.fields.map(f => f.level === 2 ? th.mow : f.level === 1 ? th.warn : th.ok),
                borderRadius: 4,
                maxBarThickness: 32,
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: { legend: { display: false } },
            scales: {
                x: { grid: { display: false }, ticks: { color: th.text, font: { size: 10 } } },
                y: { grid: { color: th.grid }, ticks: { color: th.text, font: { size: 11 }, callback: v => v + " mm" }, beginAtZero: true }
            }
        }
    });
}

function renderActivity() {
    const list = document.getElementById("activity-list");
    if (!reportData || !reportData.fields.length) {
        list.innerHTML = '<li class="px-5 py-3 text-xs text-zinc-500">Geen activiteit</li>';
        return;
    }

    // show recent fields that require attention
    const recent = reportData.fields.filter(f => f.level > 0).slice(0, 6);
    if (!recent.length) {
        list.innerHTML = '<li class="px-5 py-3 text-xs text-zinc-500">Geen actie vereist, alles ziet er goed uit.</li>';
        return;
    }

    list.innerHTML = recent.map(f => {
        const color = f.level === 2 ? "#ef4444" : "#f59e0b";
        return `<li class="flex items-start gap-3 px-5 py-3 border-t border-black/5 dark:border-white/10 first:border-t-0">
            <span class="w-2 h-2 rounded-full mt-1.5 shrink-0" style="background:${color}"></span>
            <div><div class="text-xs font-semibold">${f.name} — <span class="tabular-nums">${f.avg} mm</span> (${f.label})</div>
            <div class="text-[11px] text-zinc-500 mt-0.5">Metingen: ${f.total} — Laatste: ${f.latest ? f.latest.slice(11, 16) : "—"}</div></div>
        </li>`;
    }).join("");
}

// manual sync trigger
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
    if (reportData) renderCharts();
});

loadData();
setInterval(loadData, 30000);