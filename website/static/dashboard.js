const CIRC = 2 * Math.PI * 60;

  function statusOf(tof, sonic) {
    const h = tof || sonic || 0;
    if (h >= 400) return { label: "Maaien", bgCls: "bg-[#fee2e2]", textCls: "text-[#991b1b]", color: "#ef4444", level: 2 };
    if (h >= 80)  return { label: "Let op",  bgCls: "bg-[#fef3c7]", textCls: "text-[#92400e]", color: "#f59e0b", level: 1 };
    return                { label: "Goed",   bgCls: "bg-[#dff0c8]", textCls: "text-[#2a5238]", color: "#3b6d11", level: 0 };
  }

  function barHtml(h) {
    const pct   = Math.min(100, Math.round(h / 8));
    const color = h >= 400 ? "#ef4444" : h >= 80 ? "#f59e0b" : "#3b6d11";
    return `<div class="w-[90px] bg-[#e8e8e2] rounded-full h-[5px] overflow-hidden">
              <div style="width:${pct}%;background:${color}" class="h-full rounded-full"></div>
            </div>
            <div class="text-[10px] text-[#5a6e60] mt-[3px]">${h} mm</div>`;
  }

  let allRows = [];
  let currentFilter = "all";

  async function loadData() {
    const res = await fetch("/api/data");
    allRows = await res.json();
    renderKPIs();
    renderTable(currentFilter);
    renderDonut();
    renderActivity();
  }

  function renderKPIs() {
    const count    = allRows.length;
    const heights  = allRows.map(r => parseInt(r.tof_mm || r.sonic_median_mm || 0));
    const avg      = heights.length ? Math.round(heights.reduce((a,b) => a+b, 0) / heights.length) : 0;
    const mowCount = allRows.filter(r => statusOf(parseInt(r.tof_mm||0), parseInt(r.sonic_median_mm||0)).level >= 2).length;
    const latest   = allRows[0]?.measured_at || "—";
    const timeStr  = latest !== "—" ? latest.slice(11,16) : "—";

    document.getElementById("kpi-fields").innerHTML   = `${count} <span class="text-[13px] font-medium text-[#5a6e60] ml-0.5">rijen</span>`;
    document.getElementById("kpi-mow-badge").textContent = `${mowCount} te maaien`;
    document.getElementById("kpi-avg").innerHTML      = `${avg} <span class="text-[13px] font-medium text-[#5a6e60] ml-0.5">mm</span>`;
    document.getElementById("kpi-avg-badge").textContent = avg >= 200 ? "Boven streefwaarde" : "Normaal";
    document.getElementById("kpi-avg-badge").className   = `inline-block px-2 py-0.5 rounded-full text-[10px] font-bold ${avg >= 200 ? "bg-[#fef3c7] text-[#92400e]" : "bg-[#dff0c8] text-[#2a5238]"}`;
    document.getElementById("kpi-readings").innerHTML = `${count} <span class="text-[13px] font-medium text-[#5a6e60] ml-0.5">pts</span>`;
    document.getElementById("kpi-time").textContent   = timeStr;
    document.getElementById("kpi-temp-badge").textContent = "Sensor actief";
  }

  function renderTable(filter) {
    const rows = allRows.filter(r => {
      const s = statusOf(parseInt(r.tof_mm||0), parseInt(r.sonic_median_mm||0));
      if (filter === "mow") return s.level >= 2;
      if (filter === "ok")  return s.level === 0;
      return true;
    }).slice(0, 20);

    const tbody = document.getElementById("fields-body");
    if (!rows.length) {
      tbody.innerHTML = '<tr><td colspan="5" class="text-center px-3.5 py-5 text-[#5a6e60] text-[12px]">Geen data</td></tr>';
      return;
    }
    tbody.innerHTML = rows.map(r => {
      const tof   = r.tof_mm != null ? parseInt(r.tof_mm) : null;
      const sonic = r.sonic_median_mm != null ? parseInt(r.sonic_median_mm) : null;
      const h     = tof || sonic || 0;
      const s     = statusOf(tof, sonic);
      const t     = r.measured_at ? r.measured_at.slice(11,16) : "—";
      const d     = r.measured_at ? r.measured_at.slice(0,10)  : "—";
      return `<tr class="cursor-pointer hover:bg-[#f2fae8] transition-colors duration-100">
        <td class="text-[12px] px-3.5 py-2.5 border-b border-[#e8e8e2] text-[#1a2e1f]">
          <div class="font-semibold text-[12px]">${t}</div>
          <div class="text-[10px] text-[#5a6e60] mt-px">${d}</div>
        </td>
        <td class="text-[12px] px-3.5 py-2.5 border-b border-[#e8e8e2] text-[#1a2e1f]">${tof  != null ? tof  : "—"}</td>
        <td class="text-[12px] px-3.5 py-2.5 border-b border-[#e8e8e2] text-[#1a2e1f]">${sonic != null ? sonic : "—"}</td>
        <td class="text-[12px] px-3.5 py-2.5 border-b border-[#e8e8e2] text-[#1a2e1f]">${barHtml(h)}</td>
        <td class="text-[12px] px-3.5 py-2.5 border-b border-[#e8e8e2] text-[#1a2e1f]">
          <span class="inline-block px-2 py-0.5 rounded-full text-[10px] font-bold ${s.bgCls} ${s.textCls}">${s.label}</span>
        </td>
      </tr>`;
    }).join("");
  }

  function filterFields(f, btn) {
    currentFilter = f;
    document.querySelectorAll(".pill").forEach(p => {
      p.classList.remove("border-[#6aa84f]", "bg-[#f2fae8]", "text-[#3b6d11]");
      p.classList.add("border-[#e8e8e2]", "bg-transparent", "text-[#5a6e60]");
    });
    btn.classList.remove("border-[#e8e8e2]", "bg-transparent", "text-[#5a6e60]");
    btn.classList.add("border-[#6aa84f]", "bg-[#f2fae8]", "text-[#3b6d11]");
    renderTable(f);
  }

  function renderDonut() {
    const counts = [0, 0, 0];
    allRows.forEach(r => counts[statusOf(parseInt(r.tof_mm||0), parseInt(r.sonic_median_mm||0)).level]++);
    const total    = allRows.length || 1;
    const segOk    = (counts[0] / total) * CIRC;
    const segWarn  = (counts[1] / total) * CIRC;
    const segAlert = (counts[2] / total) * CIRC;

    document.getElementById("donut-ok")   .setAttribute("stroke-dasharray", `${segOk} ${CIRC - segOk}`);
    document.getElementById("donut-ok")   .setAttribute("stroke-dashoffset", "0");
    document.getElementById("donut-warn") .setAttribute("stroke-dasharray", `${segWarn} ${CIRC - segWarn}`);
    document.getElementById("donut-warn") .setAttribute("stroke-dashoffset", `${-segOk}`);
    document.getElementById("donut-alert").setAttribute("stroke-dasharray", `${segAlert} ${CIRC - segAlert}`);
    document.getElementById("donut-alert").setAttribute("stroke-dashoffset", `${-segOk - segWarn}`);
    document.getElementById("donut-pct").textContent = Math.round((counts[0] / total) * 100) + "%";
  }

  function renderActivity() {
    const recent = allRows.slice(0, 5);
    const list   = document.getElementById("activity-list");
    list.innerHTML = recent.map(r => {
      const h = parseInt(r.tof_mm || r.sonic_median_mm || 0);
      const s = statusOf(parseInt(r.tof_mm||0), parseInt(r.sonic_median_mm||0));
      const t = r.measured_at ? r.measured_at.slice(11,16) : "—";
      return `<li class="flex items-start gap-2.5 px-[18px] py-2.5 border-b border-[#e8e8e2] last:border-b-0">
        <span class="inline-block w-[7px] h-[7px] rounded-full mt-1 shrink-0" style="background:${s.color}"></span>
        <div>
          <div class="text-[11px] text-[#1a2e1f] leading-[1.5]">Meting ontvangen — ${h}mm (${s.label})</div>
          <div class="text-[10px] text-[#5a6e60] mt-0.5">${t}</div>
        </div>
      </li>`;
    }).join("");
  }

  loadData();
  setInterval(loadData, 30000);