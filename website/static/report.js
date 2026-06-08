let fullData = null;
let barChart = null;
let donutChart = null;

async function loadReport() {
    const res = await fetch("/api/summary");
    fullData = await res.json();

    const now = new Date().toLocaleDateString("nl-NL", { day: "numeric", month: "long", year: "numeric" });
    const src = fullData.source === "arcgis" ? "ArcGIS" : "Geen bron";
    document.getElementById("report-date").textContent = `Gegenereerd op ${now} · Bron: ${src}`;

    const sel = document.getElementById("reportFieldSelect");
    fullData.fields.forEach(f => {
        const opt = document.createElement("option");
        opt.value = f.id; opt.textContent = f.name;
        sel.appendChild(opt);
    });

    const urlParams = new URLSearchParams(window.location.search);
    const initialField = urlParams.get('field');
    if (initialField !== null) {
        sel.value = initialField;
        renderUI(initialField);
    } else {
        renderUI("");
    }
}

document.getElementById("reportFieldSelect").addEventListener("change", (e) => { renderUI(e.target.value); });

function renderUI(fieldId) {
    let d = fullData;

    if (fieldId !== "") {
        const f = fullData.fields.find(x => String(x.id) === String(fieldId));
        if (f) {
            const c = [0,0,0,0,0];
            f.history.forEach(h => { c[h.lvl]++; });

            d = { total: f.total, counts: c, pcts: [0,0,0,0,0], latest: f.latest, fields: [f], title: "Rapport: " + f.name };
            const histTotal = f.history.length || 1;
            for(let i=0; i<5; i++) d.pcts[i] = Math.round((c[i] / histTotal) * 100);
        }

        document.getElementById("chartLabelLeft").textContent = "Metingen Verloop";
        document.getElementById("chartSubLeft").textContent = "Historie van maaibeurten en metingen";
        document.getElementById("chartLabelRight").textContent = "Historische Kwaliteit";
        document.getElementById("table-title").textContent = "Metingen & Maaibeurten";
        renderHistory(d.fields[0]);
        renderSingleCharts(d);
    } else {
        d.title = "Veldbeheer Rapport (Totaal)";
        document.getElementById("chartLabelLeft").textContent = "Grashoogte";
        document.getElementById("chartSubLeft").textContent = "Gemiddelde per veld";
        document.getElementById("chartLabelRight").textContent = "Kwaliteitsverdeling";
        document.getElementById("table-title").textContent = "Veld details";
        renderRows(d.fields);
        renderCharts(d);
    }

    document.getElementById("r-title").textContent = d.title;
    document.getElementById("r-total").textContent = d.total;
    document.getElementById("r-ok").textContent = d.counts[0] + d.counts[1];
    document.getElementById("r-mow").textContent = d.counts[4];

    renderRecs(d.counts, fieldId !== "");
}

function renderRows(fields) {
    if (!fields.length) { document.getElementById("report-rows").innerHTML = '<div class="px-5 py-4 text-zinc-500">Geen velden gevonden.</div>'; return; }
    const bgs = ["text-bg-success", "text-bg-success", "text-bg-warning", "text-bg-warning", "text-bg-danger"];
    document.getElementById("report-rows").innerHTML = fields.map(f => `
        <div class="flex items-center justify-between py-3 border-b border-black/5 dark:border-white/10 last:border-0">
            <div>
                <div class="font-semibold">${f.name}</div>
                <div class="text-[11px] text-zinc-500">Laatste: ${f.latest ? f.latest.slice(0, 16) : "—"}</div>
                <div class="text-[11px] mt-1 font-medium text-brand">Advies: ${f.action}</div>
            </div>
            <div class="flex items-center gap-4"><div class="text-right"><div class="font-semibold tabular-nums">${f.avg} mm</div><div class="text-[10px] text-zinc-500">${f.total} metingen</div></div>
            <span class="badge rounded-pill ${bgs[f.level]} w-10 text-center">${f.label}</span></div>
        </div>`).join("");
}

function renderHistory(f) {
    if (!f.history || !f.history.length) { document.getElementById("report-rows").innerHTML = '<div class="px-5 py-4 text-zinc-500">Geen metingen beschikbaar.</div>'; return; }
    let html = "";
    f.history.forEach((r, i) => {
        const countLabel = r.count > 1 ? `gem. van ${r.count} metingen` : `1 meting`;
        html += `
        <div class="flex items-center justify-between py-3 border-b border-black/5 dark:border-white/10 last:border-0">
            <div>
                <div class="font-semibold ${r.is_mow ? 'text-brand' : ''}">${r.title}</div>
                <div class="text-[11px] text-zinc-500">${r.date} ${r.time} · <span class="italic">${countLabel}</span></div>
            </div>
            <div class="font-semibold tabular-nums">${r.h} mm</div>
        </div>`;
    });
    document.getElementById("report-rows").innerHTML = html;
}

function renderCharts(d) {
    const th = window.chartTheme();
    const colors = [th.aplus, th.a, th.b, th.c, th.d];

    if (barChart) barChart.destroy();
    barChart = new Chart(document.getElementById("barChart"), {
        type: "bar",
        data: { labels: d.fields.map(f => f.name), datasets: [{ label: "Grashoogte", data: d.fields.map(f => f.avg), backgroundColor: d.fields.map(f => colors[f.level]), borderRadius: 4, maxBarThickness: 32 }] },
        options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false }, tooltip: { callbacks: { label: c => `Grashoogte: ${c.parsed.y} mm` } } }, scales: { x: { grid: { display: false }, ticks: { color: th.text } }, y: { grid: { color: th.grid }, ticks: { color: th.text, callback: v => v + " mm" }, beginAtZero: true } } }
    });

    if (donutChart) donutChart.destroy();
    donutChart = new Chart(document.getElementById("donutChart"), {
        type: "doughnut",
        data: { labels: ["A+", "A", "B", "C", "D"], datasets: [{ data: d.counts, backgroundColor: colors, borderWidth: 0 }] },
        options: { responsive: true, maintainAspectRatio: false, cutout: "72%", plugins: { legend: { display: false } } }
    });
    document.getElementById("donut-pct").textContent = (d.pcts[0] + d.pcts[1]) + "%";
}

function renderSingleCharts(d) {
    const th = window.chartTheme();
    const colors = [th.aplus, th.a, th.b, th.c, th.d];
    const f = d.fields[0];

    if (barChart) barChart.destroy();
    barChart = new Chart(document.getElementById("barChart"), {
        type: "line",
        data: { labels: f.history.map(r => r.date.slice(5)), datasets: [{ label: "Grashoogte", data: f.history.map(r => r.h), borderColor: th.brand, backgroundColor: "rgba(59,109,17,0.15)", fill: true, tension: 0.3, pointRadius: 4, pointBackgroundColor: f.history.map(r => r.is_mow ? th.brand : th.a) }] },
        options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false }, tooltip: { callbacks: { label: c => `Grashoogte: ${c.parsed.y} mm` } } }, scales: { x: { ticks: { color: th.text } }, y: { grid: { color: th.grid }, ticks: { color: th.text, callback: v => v + " mm" }, beginAtZero: true } } }
    });

    if (donutChart) donutChart.destroy();
    donutChart = new Chart(document.getElementById("donutChart"), {
        type: "doughnut",
        data: { labels: ["A+", "A", "B", "C", "D"], datasets: [{ data: d.counts, backgroundColor: colors, borderWidth: 0 }] },
        options: { responsive: true, maintainAspectRatio: false, cutout: "72%", plugins: { legend: { display: false } } }
    });
    document.getElementById("donut-pct").textContent = (d.pcts[0] + d.pcts[1]) + "%";
}

function renderRecs(counts, isSingle) {
    const plural = (n, s, p) => n === 1 ? s : p;
    let html = isSingle ? "Op basis van het historische verloop van dit veld:<br><br>" : "Op basis van de actuele data van alle velden:<br><br>";
    if (counts[4]) html += `<strong>${counts[4]} ${plural(counts[4], "keer/meting", "keer/metingen")}</strong> (D) onder de maat (>100mm) — directe maaiactie noodzakelijk.<br>`;
    if (counts[3]) html += `<strong>${counts[3]} ${plural(counts[3], "keer/meting", "keer/metingen")}</strong> (C) matig (90-100mm) — maaien inplannen.<br>`;
    if (counts[2]) html += `<strong>${counts[2]} ${plural(counts[2], "keer/meting", "keer/metingen")}</strong> (B) acceptabel (80-90mm) — visueel controleren.<br>`;
    if (counts[0] + counts[1]) html += `<strong>${counts[0] + counts[1]} ${plural(counts[0] + counts[1], "keer/meting", "keer/metingen")}</strong> in A/A+ (≤80mm) — gras is in topconditie.<br>`;
    html += `<br><em class="text-xs text-zinc-500">Kwaliteitseisen: A+ (≤70), A (≤80), B (≤90), C (≤100), D (>100) mm.</em>`;
    document.getElementById("report-recs").innerHTML = html;
}

document.getElementById("downloadBtn").addEventListener("click", async function () {
    const btn = this; const original = btn.textContent;
    btn.textContent = "Genereren…"; btn.disabled = true;
    try {
        const fieldId = document.getElementById("reportFieldSelect").value;
        const res = await fetch(`/download-pdf` + (fieldId ? `?field=${fieldId}` : ""));
        if (!res.ok) throw new Error(`Server gaf statuscode ${res.status}`);
        const blob = await res.blob();
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url; a.download = "veldbeheer-rapport.pdf";
        document.body.appendChild(a); a.click(); a.remove(); URL.revokeObjectURL(url);
    } catch (err) { alert(`Fout bij downloaden: ${err.message}`); } finally { btn.textContent = original; btn.disabled = false; }
});

window.addEventListener("themechange", () => { if (fullData) renderUI(document.getElementById("reportFieldSelect").value); });
loadReport();