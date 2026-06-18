let reportData = null;
let barChart = null;
let donutChart = null;

async function loadReportData(days = "30", fieldId = "") {
    try {
        const res = await fetch(`/api/summary?days=${days}&field_id=${fieldId}`);
        reportData = await res.json();
        renderReport();
        renderCharts();
    } catch (e) {
        document.getElementById("report-rows").innerHTML = '<div class="px-4 py-6 text-center text-[#ef4444] text-sm">Kon rapportage data niet laden</div>';
    }
}

document.getElementById("daysFilter").addEventListener("change", (e) => {
    localStorage.setItem("daysFilter", e.target.value);
    const fieldId = document.getElementById("reportFieldSelect").value;
    loadReportData(e.target.value, fieldId);
});

document.getElementById("reportFieldSelect").addEventListener("change", (e) => {
    const days = document.getElementById("daysFilter").value;
    loadReportData(days, e.target.value);
});

function gradeStyle(level) {
    const th = window.chartTheme();
    const colors = [th.aplus, th.a, th.b, th.c, th.d];
    return { color: colors[level] };
}

function renderReport() {
    if (!reportData) return;

    document.getElementById("report-date").textContent = `Gegenereerd op ${reportData.generated} • Laatste meting: ${reportData.latest}`;
    document.getElementById("r-total").textContent = reportData.total;
    document.getElementById("r-ok").textContent = reportData.counts[0] + reportData.counts[1];
    document.getElementById("r-mow").textContent = reportData.counts[4];

    const rowsEl = document.getElementById("report-rows");

    if (reportData.target_field) {
        document.getElementById("r-title").textContent = `Rapport: ${reportData.fields[0]?.name || 'Veld'}`;
        document.getElementById("chartLabelLeft").textContent = "Metingen & Maaibeurten";
        document.getElementById("chartSubLeft").textContent = "Tijdlijn van de meest recente sensordata";

        let html = `<div class="hidden md:block w-full overflow-x-auto min-w-0"><table class="w-full text-sm text-left"><thead class="sticky top-0 bg-white/95 dark:bg-[#18181b]/95 backdrop-blur z-10 border-b border-black/5 dark:border-white/10 text-[10px] uppercase tracking-[0.08em] text-zinc-500 dark:text-zinc-400"><th class="px-4 py-2.5 font-bold">Type</th><th class="px-4 py-2.5 font-bold">Datum & Tijd</th><th class="px-4 py-2.5 font-bold">Hoogte</th><th class="px-4 py-2.5 font-bold">Metingen</th></thead><tbody class="divide-y divide-black/5 dark:divide-white/5">`;

        if(reportData.fields[0]) {
            html += reportData.fields[0].history.map(h => {
                const isMow = h.is_mow;
                return `<tr>
                    <td class="px-4 py-2.5 font-semibold ${isMow ? 'text-brand dark:text-[#7bc53b]' : ''}">${h.title}</td>
                    <td class="px-4 py-2.5 text-zinc-500 tabular-nums">${h.time}</td>
                    <td class="px-4 py-2.5 tabular-nums">${h.h} mm</td>
                    <td class="px-4 py-2.5 text-zinc-500 text-xs">gem. van ${h.count} meting${h.count === 1 ? '' : 'en'}</td>
                </tr>`;
            }).join("");
        }
        html += `</tbody></table></div>`;

        // Mobile card layout (avoids forcing a wide 4-column table into a narrow card)
        html += `<ul class="md:hidden m-0 p-0 list-none flex flex-col w-full min-w-0">`;
            html += reportData.fields[0].history.map(h => {
                const isMow = h.is_mow;
                return `<li class="flex items-center justify-between gap-3 px-4 py-2.5 border-t border-black/5 dark:border-white/10 first:border-t-0">
                    <div class="min-w-0 flex-1">
                        <div class="font-semibold text-sm truncate ${isMow ? 'text-brand dark:text-[#7bc53b]' : ''}">${h.title}</div>
                        <div class="text-[11px] text-zinc-500 mt-0.5 truncate">${h.time} · gem. van ${h.count} meting${h.count === 1 ? '' : 'en'}</div>
                    </div>
                    <div class="text-sm font-semibold tabular-nums shrink-0">${h.h} mm</div>
                </li>`;
            }).join("");
        html += `</ul>`;
        rowsEl.innerHTML = html;

    } else {
        document.getElementById("r-title").textContent = `Veldbeheer Rapport (Totaal)`;
        document.getElementById("chartLabelLeft").textContent = "Grashoogte";
        document.getElementById("chartSubLeft").textContent = "Gemiddelde hoogte per veld";

        // Desktop table
        let html = `
        <div class="hidden md:block w-full overflow-x-auto min-w-0">
        <table class="w-full text-sm text-left"><thead class="sticky top-0 bg-white/95 dark:bg-[#18181b]/95 backdrop-blur z-10 border-b border-black/5 dark:border-white/10 text-[10px] uppercase tracking-[0.08em] text-zinc-500 dark:text-zinc-400"><th class="px-4 py-2.5 font-bold">Veldnaam</th><th class="px-4 py-2.5 font-bold">Laatst</th><th class="px-4 py-2.5 font-bold">Actie</th><th class="px-4 py-2.5 font-bold text-center">Kwaliteit</th></thead><tbody class="divide-y divide-black/5 dark:divide-white/5">`;
        html += reportData.fields.map(f => {
            const style = gradeStyle(f.level);
            return `<tr>
                <td class="px-4 py-2.5 font-semibold">${f.name}</td>
                <td class="px-4 py-2.5 text-[11px] text-zinc-500 tabular-nums">${f.latest ? f.latest.slice(0, 16) : "—"}</td>
                <td class="px-4 py-2.5 font-medium ${f.level >= 3 ? '' : 'text-zinc-500'}" style="${f.level >= 3 ? `color:${style.color}` : ''}">${f.action}</td>
                <td class="px-4 py-2.5 text-center"><span class="badge rounded-pill" style="background-color:${style.color}; color:#fff; border:none; width:40px;">${f.label}</span></td>
            </tr>`;
        }).join("");
        html += `</tbody></table></div>`;

        // Mobile card layout matching dashboard style
        html += `<ul class="md:hidden m-0 p-0 list-none flex flex-col w-full min-w-0">`;
        html += reportData.fields.map(f => {
            const style = gradeStyle(f.level);
            return `<li class="flex items-center gap-3 px-4 py-2.5 border-t border-black/5 dark:border-white/10 first:border-t-0">
                <span class="badge rounded-pill shrink-0 w-9 text-center" style="background-color:${style.color}; color:#fff; border:none;">${f.label}</span>
                <div class="flex-1 min-w-0">
                    <div class="font-semibold text-sm truncate">${f.name}</div>
                    <div class="text-[11px] text-zinc-500 mt-0.5 truncate">${f.latest ? f.latest.slice(0, 16) : "—"} · ${f.action}</div>
                </div>
            </li>`;
        }).join("");
        html += `</ul>`;

        rowsEl.innerHTML = html;
    }

    const recsEl = document.getElementById("report-recs");
    let recsHtml = `<div class="space-y-3">`;
    if (reportData.counts[4]) recsHtml += `<div><strong class="text-zinc-800 dark:text-zinc-200">${reportData.counts[4]} meting(en)</strong> met kwaliteit D (>100 mm). Direct maaien vereist!</div>`;
    if (reportData.counts[3]) recsHtml += `<div><strong class="text-zinc-800 dark:text-zinc-200">${reportData.counts[3]} meting(en)</strong> met kwaliteit C (90-100 mm). Plan op korte termijn een maaibeurt.</div>`;
    if (reportData.counts[2]) recsHtml += `<div><strong class="text-zinc-800 dark:text-zinc-200">${reportData.counts[2]} meting(en)</strong> met kwaliteit B (80-90 mm). Acceptabel, hou de groei in de gaten.</div>`;
    if (reportData.counts[0] + reportData.counts[1]) recsHtml += `<div><strong class="text-zinc-800 dark:text-zinc-200">${reportData.counts[0] + reportData.counts[1]} meting(en)</strong> in topconditie A/A+ (≤80 mm). Geen actie nodig.</div>`;

    recsHtml += `<div class="text-[10px] text-zinc-400 mt-4 pt-4 border-t border-black/5 dark:border-white/10">Kwaliteitseisen conform normering: A+ (≤70), A (≤80), B (≤90), C (≤100), D (>100) mm.</div></div>`;
    recsEl.innerHTML = recsHtml;
}

function renderCharts() {
    if (!reportData) return;
    const th = window.chartTheme();
    const colors = [th.aplus, th.a, th.b, th.c, th.d];

    const ctx = document.getElementById("barChart");
    if (barChart) barChart.destroy();

    if (reportData.target_field && reportData.fields[0]) {
        const f = reportData.fields[0];
        barChart = new Chart(ctx, {
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
        barChart = new Chart(ctx, {
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

    const counts = reportData.counts;
    const total = counts.reduce((a, b) => a + b, 0);

    const ctx2 = document.getElementById("donutChart");
    if (donutChart) donutChart.destroy();
    donutChart = new Chart(ctx2, {
        type: "doughnut",
        data: { labels: ["A+", "A", "B", "C", "D"], datasets: [{ data: counts, backgroundColor: colors, borderWidth: 0 }] },
        options: { responsive: true, maintainAspectRatio: false, cutout: "72%", plugins: { legend: { display: false } } }
    });

    const pct = total ? Math.round(((counts[0] + counts[1]) / total) * 100) : 0;
    const pctEl = document.getElementById("donut-pct");
    if (pctEl) pctEl.textContent = pct + "%";
}

async function initReportFields(initialFieldId) {
    try {
        const days = document.getElementById("daysFilter").value || "30";
        const res = await fetch(`/api/summary?days=${days}`);
        const json = await res.json();
        const sel = document.getElementById("reportFieldSelect");

        sel.innerHTML = '<option value="">Alle velden</option>';

        json.fields.forEach(f => {
            const opt = document.createElement("option");
            opt.value = f.id;
            opt.textContent = f.name;
            sel.appendChild(opt);
        });

        if (initialFieldId) {
            sel.value = initialFieldId;
        }
    } catch (e) {
        console.error(e);
    }
}

const downloadBtn = document.getElementById("downloadBtn");
if(downloadBtn) {
    downloadBtn.onclick = () => {
        const sel = document.getElementById("reportFieldSelect").value;
        const days = document.getElementById("daysFilter").value;
        let url = `/download-pdf?days=${days}`;
        if (sel) url += `&field=${sel}`;
        window.open(url, "_blank");
    };
}

window.addEventListener("themechange", () => { if (reportData) { renderCharts(); } });

(async function init() {
    const urlParams = new URLSearchParams(window.location.search);
    const initialField = urlParams.get('field') || "";
    const urlDays = urlParams.get('days');

    const df = document.getElementById("daysFilter");

    if (urlDays !== null) {
        if (df) df.value = urlDays;
        localStorage.setItem("daysFilter", urlDays);
    } else {
        const savedDays = localStorage.getItem("daysFilter");
        if (savedDays !== null && df) {
            df.value = savedDays;
        }
    }

    const days = df ? df.value : "30";

    await initReportFields(initialField);

    loadReportData(days, initialField);
})();