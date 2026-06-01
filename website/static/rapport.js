function statusOf(tof, sonic) {
    const h = tof || sonic || 0;
    if (h >= 400) return { label: "Maaien", bg: "bg-[#fee2e2]", text: "text-[#991b1b]", level: 2 };
    if (h >= 80)  return { label: "Let op",  bg: "bg-[#fef3c7]", text: "text-[#92400e]", level: 1 };
    return               { label: "Goed",   bg: "bg-[#dff0c8]", text: "text-[#2a5238]", level: 0 };
  }

  async function loadReport() {
    const res  = await fetch("/api/data");
    const rows = await res.json();

    const now = new Date().toLocaleDateString("nl-NL", { day: "numeric", month: "long", year: "numeric" });
    document.getElementById("report-date").textContent = `Gegenereerd op ${now} · Prototype v1.0`;

    const counts = [0, 0, 0];
    rows.forEach(r => counts[statusOf(parseInt(r.tof_mm || 0), parseInt(r.sonic_median_mm || 0)).level]++);

    document.getElementById("r-total").textContent = rows.length;
    document.getElementById("r-ok").textContent    = counts[0];
    document.getElementById("r-mow").textContent   = counts[2];

    document.getElementById("report-rows").innerHTML = rows.slice(0, 20).map(r => {
      const tof   = r.tof_mm != null ? parseInt(r.tof_mm) : null;
      const sonic = r.sonic_median_mm != null ? parseInt(r.sonic_median_mm) : null;
      const h     = tof || sonic || 0;
      const s     = statusOf(tof, sonic);
      const time  = r.measured_at ? r.measured_at.slice(11, 16) : "—";
      const date  = r.measured_at ? r.measured_at.slice(0, 10)  : "—";
      return `<div class="flex items-center justify-between py-2.5 border-b border-[#e8e8e2] last:border-b-0 text-[12px]">
        <div>
          <div class="font-semibold text-[#1a2e1f]">${time}</div>
          <div class="text-[10px] text-[#5a6e60] mt-px">${date}</div>
        </div>
        <div class="flex items-center gap-2.5">
          <div class="text-[14px] font-bold text-[#1a2e1f]">${h} mm</div>
          <span class="inline-block px-2 py-0.5 rounded-full text-[10px] font-bold ${s.bg} ${s.text}">${s.label}</span>
        </div>
      </div>`;
    }).join("");

    const mowCount  = counts[2];
    const warnCount = counts[1];
    document.getElementById("report-recs").innerHTML = `
      Op basis van de sensordata van vandaag:<br><br>
      ${mowCount  > 0 ? `<strong class="text-[#1a2e1f]">${mowCount} meting${mowCount > 1 ? "en" : ""}</strong> tonen een grashoogte boven 400mm — directe maaiactie aanbevolen.<br>` : ""}
      ${warnCount > 0 ? `<strong class="text-[#1a2e1f]">${warnCount} meting${warnCount > 1 ? "en" : ""}</strong> tonen een grashoogte tussen 80–400mm — nauwlettend in de gaten houden.<br>` : ""}
      ${counts[0] > 0 ? `<strong class="text-[#1a2e1f]">${counts[0]} meting${counts[0] > 1 ? "en" : ""}</strong> ${counts[0] > 1 ? "zijn" : "is"} binnen de normale waarden.<br>` : ""}
      <br><em class="text-[#5a6e60] text-[11px]">Dit rapport is gegenereerd op basis van live sensordata. Drempelwaarden: ≥400mm = maaien, ≥80mm = let op.</em>
    `;
  }

  loadReport();