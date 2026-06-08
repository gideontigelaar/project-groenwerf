// prepare and render data for the web report view
const MOW = 400;
const WATCH = 80;

function statusOf(tof, sonic) {
    const h = (tof || sonic || 0);
    if (h >= MOW) return { label: "Maaien", level: 2, color: "#ef4444", badge: "text-bg-danger" };
    if (h >= WATCH) return { label: "Let op", level: 1, color: "#f59e0b", badge: "text-bg-warning" };
    return { label: "Goed", level: 0, color: "#6aa84f", badge: "text-bg-success" };
}

const heightOf = r => (r.tof_mm || r.sonic_mm || 0);

let rows = [];
let barChart = null;
let donutChart = null;

async function loadReport() {
    const res = await fetch("/api/data");
    const json = await res.json();
    rows = json.data || [];

    const now = new Date().toLocaleDateString("nl-NL", { day: "numeric", month: "long", year: "numeric" });
    const src = json.meta && json.meta.source === "arcgis" ? "ArcGIS" : "Geen bron";
    document.getElementById("report-date").textContent = `Gegenereerd op ${now} · Bron: ${src}`;

    const counts = [0, 0, 0];
    rows.forEach(r => counts[statusOf(r.tof_mm, r.sonic_mm).level]++);
    document.getElementById("r-total").textContent = rows.length;
    document.getElementById("r-ok").textContent = counts[0];
    document.getElementById("r-mow").textContent = counts[2];

    renderRows();
    renderCharts(counts);
    renderRecs(counts);
}

function renderRows() {
    document.getElementById("report-rows").innerHTML = rows.slice(0, 20).map(r => {
        const h = heightOf(r);
        const s = statusOf(r.tof_mm, r.sonic_mm);
        const t = r.measured_at ? r.measured_at.slice(11, 16) : "—";
        const d = r.measured_at ? r.measured_at.slice(0, 10) : "—";
        return `<div class="flex items-center justify-between py-2.5 border-b border-black/5 dark:border-white/10 last:border-0">
            <div><div class="font-semibold">${t}</div><div class="text-[11px] text-zinc-500">${d}</div></div>
            <div class="flex items-center gap-3"><div class="font-semibold tabular-nums">${h} mm</div>
            <span class="badge rounded-pill ${s.badge}">${s.label}</span></div>
        </div>`;
    }).join("");
}

function renderCharts(counts) {
    const th = window.chartTheme();
    const recent = rows.slice(0, 18).reverse();

    if (barChart) barChart.destroy();
    barChart = new Chart(document.getElementById("barChart"), {
        type: "bar",
        data: {
            labels: recent.map(r => (r.measured_at || "").slice(11, 16)),
            datasets: [{
                data: recent.map(heightOf),
                backgroundColor: recent.map(r => statusOf(r.tof_mm, r.sonic_mm).color),
                borderRadius: 5,
                maxBarThickness: 26,
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
                x: { grid: { display: false }, ticks: { color: th.text, font: { size: 10 }, maxTicksLimit: 9 } },
                y: { grid: { color: th.grid }, ticks: { color: th.text, font: { size: 11 }, callback: v => v + " mm" }, beginAtZero: true }
            }
        }
    });

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
            cutout: "68%",
            plugins: { legend: { position: "bottom", labels: { color: th.text, boxWidth: 10, font: { size: 11 } } } }
        }
    });
}

function renderRecs(counts) {
    const [ok, warn, mow] = counts;
    const plural = (n, s, p) => n === 1 ? s : p;
    let html = "Op basis van de sensordata van vandaag:<br><br>";
    if (mow) html += `<strong>${mow} ${plural(mow, "meting", "metingen")}</strong> ${plural(mow, "toont", "tonen")} een grashoogte boven ${MOW} mm — directe maaiactie aanbevolen.<br>`;
    if (warn) html += `<strong>${warn} ${plural(warn, "meting", "metingen")}</strong> ${plural(warn, "ligt", "liggen")} tussen ${WATCH}–${MOW} mm — nauwlettend volgen.<br>`;
    if (ok) html += `<strong>${ok} ${plural(ok, "meting", "metingen")}</strong> ${plural(ok, "is", "zijn")} binnen de normale waarden.<br>`;
    html += `<br><em class="text-xs text-zinc-500">Drempelwaarden: ≥${MOW} mm = maaien, ≥${WATCH} mm = let op.</em>`;
    document.getElementById("report-recs").innerHTML = html;
}

document.getElementById("downloadBtn").addEventListener("click", async function () {
    const btn = this;
    const original = btn.textContent;
    btn.textContent = "Genereren…";
    btn.disabled = true;
    try {
        const res = await fetch("/download-pdf");
        if (!res.ok) throw new Error(`Server gaf statuscode ${res.status}`);
        const blob = await res.blob();
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = "veldbeheer-rapport.pdf";
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(url);
    } catch (err) {
        alert(`Fout bij downloaden: ${err.message}`);
    } finally {
        btn.textContent = original;
        btn.disabled = false;
    }
});

window.addEventListener("themechange", () => {
    if (rows.length) {
        const c = [0, 0, 0];
        rows.forEach(r => c[statusOf(r.tof_mm, r.sonic_mm).level]++);
        renderCharts(c);
    }
});

loadReport();