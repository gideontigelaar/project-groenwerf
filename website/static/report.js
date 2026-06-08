// render report view based on field summaries
let reportData = null;
let barChart = null;
let donutChart = null;

async function loadReport() {
    const res = await fetch("/api/summary");
    reportData = await res.json();

    const now = new Date().toLocaleDateString("nl-NL", { day: "numeric", month: "long", year: "numeric" });
    const src = reportData.source === "arcgis" ? "ArcGIS" : "Geen bron";
    document.getElementById("report-date").textContent = `Gegenereerd op ${now} · Bron: ${src}`;

    document.getElementById("r-total").textContent = reportData.total;
    document.getElementById("r-ok").textContent = reportData.counts_ok;
    document.getElementById("r-mow").textContent = reportData.counts_mow;

    renderRows();
    renderCharts();
    renderRecs();
}

function renderRows() {
    if (!reportData.fields.length) {
        document.getElementById("report-rows").innerHTML = '<div class="px-5 py-4 text-zinc-500">Geen velden gevonden.</div>';
        return;
    }

    document.getElementById("report-rows").innerHTML = reportData.fields.map(f => {
        const badge = f.level === 2 ? "text-bg-danger" : f.level === 1 ? "text-bg-warning" : "text-bg-success";
        return `<div class="flex items-center justify-between py-3 border-b border-black/5 dark:border-white/10 last:border-0">
            <div><div class="font-semibold">${f.name}</div><div class="text-[11px] text-zinc-500">Laatste: ${f.latest ? f.latest.slice(0, 16) : "—"}</div></div>
            <div class="flex items-center gap-4"><div class="text-right"><div class="font-semibold tabular-nums">${f.avg} mm</div><div class="text-[10px] text-zinc-500">${f.total} metingen</div></div>
            <span class="badge rounded-pill ${badge} w-16 text-center">${f.label}</span></div>
        </div>`;
    }).join("");
}

function renderCharts() {
    const th = window.chartTheme();

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
            cutout: "68%",
            plugins: { legend: { position: "bottom", labels: { color: th.text, boxWidth: 10, font: { size: 11 } } } }
        }
    });
}

function renderRecs() {
    const ok = reportData.counts_ok;
    const warn = reportData.counts_warn;
    const mow = reportData.counts_mow;
    const plural = (n, s, p) => n === 1 ? s : p;

    let html = "Op basis van de ruimtelijke sensordata:<br><br>";
    if (mow) html += `<strong>${mow} ${plural(mow, "meting", "metingen")}</strong> verspreid over de velden tonen een grashoogte boven de streefwaarde — directe maaiactie aanbevolen.<br>`;
    if (warn) html += `<strong>${warn} ${plural(warn, "meting", "metingen")}</strong> bevinden zich in de waarschuwingszone — nauwlettend volgen.<br>`;
    if (ok) html += `<strong>${ok} ${plural(ok, "meting", "metingen")}</strong> zijn binnen de normale waarden.<br>`;
    html += `<br><em class="text-xs text-zinc-500">Zie de tabel hierboven voor een specificatie per veld.</em>`;
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
    if (reportData) renderCharts();
});

loadReport();