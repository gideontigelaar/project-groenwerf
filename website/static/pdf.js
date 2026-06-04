/* ── Tailwind-compatible helper classes injected via JS (for dynamically generated table rows) ── */
function getStatus(h) {
  if (h >= 90) return { label:'Maaien', badgeCls:'bg-alert-red-light text-red-800', fill:'fill-alert', fillColor:'#ef4444' };
  if (h >= 65) return { label:'Let op',  badgeCls:'bg-amber-light text-amber-800',  fill:'fill-warn',  fillColor:'#f59e0b' };
  return               { label:'Goed',   badgeCls:'bg-green-100 text-green-700',    fill:'fill-ok',    fillColor:'#4d8c18' };
}

const now = new Date();
const fmt     = d => d.toLocaleDateString('nl-NL', { weekday:'long', year:'numeric', month:'long', day:'numeric' });
const fmtTime = d => d.toLocaleTimeString('nl-NL', { hour:'2-digit', minute:'2-digit' });
document.getElementById('report-date').textContent  = fmt(now);
document.getElementById('last-meas').textContent    = fmtTime(now);
document.getElementById('footer-date').textContent  = fmt(now);

const fields = [
  { id:'A', name:'Hooiland Noord',    tof:112, sonic:108, growth:4.2, lat:51.438, lng:6.082 },
  { id:'B', name:'Sportveldje',       tof:72,  sonic:75,  growth:3.1, lat:51.442, lng:6.079 },
  { id:'C', name:'Inrijpad West',     tof:41,  sonic:38,  growth:1.4, lat:51.436, lng:6.075 },
  { id:'D', name:'Weideperceel Oost', tof:98,  sonic:101, growth:3.8, lat:51.440, lng:6.088 },
  { id:'E', name:'Centraal Grasland', tof:68,  sonic:65,  growth:5.0, lat:51.444, lng:6.085 },
  { id:'F', name:'Buitenrand Zuid',   tof:94,  sonic:90,  growth:2.9, lat:51.433, lng:6.080 },
];

const weekDays = ['Ma','Di','Wo','Do','Vr','Za','Zo'];
const weekData = {
  A: [85,90,96,102,107,110,112],
  B: [50,54,58,62,65,70,72],
  C: [55,48,45,44,42,41,41],
  D: [72,76,80,85,90,95,98],
  E: [38,44,50,55,60,65,68],
  F: [68,72,76,80,85,90,94],
};

// BAR CHART
new Chart(document.getElementById('barChart'), {
  type: 'bar',
  data: {
    labels: fields.map(f => 'Veld ' + f.id),
    datasets: [
      {
        label: 'TOF (mm)',
        data: fields.map(f => f.tof),
        backgroundColor: fields.map(f => f.tof >= 90 ? '#ef4444' : f.tof >= 65 ? '#f59e0b' : '#3b6d11'),
        borderRadius: 4,
      },
      {
        label: 'Sonic (mm)',
        data: fields.map(f => f.sonic),
        backgroundColor: fields.map(f => f.sonic >= 90 ? '#fca5a5' : f.sonic >= 65 ? '#fde68a' : '#94c66d'),
        borderRadius: 4,
        borderWidth: 1,
        borderColor: fields.map(f => f.sonic >= 90 ? '#ef4444' : f.sonic >= 65 ? '#f59e0b' : '#3b6d11'),
      }
    ]
  },
  options: {
    responsive: true, maintainAspectRatio: false,
    plugins: {
      legend: { display: false },
      tooltip: { callbacks: { label: ctx => ctx.dataset.label + ': ' + ctx.parsed.y + ' mm' } }
    },
    scales: {
      x: { grid: { display: false }, ticks: { font: { size: 11 }, color: '#5a6e60' } },
      y: { beginAtZero: true, max: 130, grid: { color: '#e8e8e2' }, ticks: { font: { size: 11 }, color: '#5a6e60', callback: v => v + ' mm' } }
    }
  }
});

// DONUT
new Chart(document.getElementById('donutChart'), {
  type: 'doughnut',
  data: {
    labels: ['Goed', 'Let op', 'Maaien'],
    datasets: [{ data: [1,2,3], backgroundColor: ['#3b6d11','#f59e0b','#ef4444'], borderWidth: 0, hoverOffset: 6 }]
  },
  options: {
    responsive: true, maintainAspectRatio: false, cutout: '68%',
    plugins: {
      legend: { display: false },
      tooltip: { callbacks: { label: ctx => ctx.label + ': ' + ctx.parsed + ' velden' } }
    }
  }
});

// LINE CHART
new Chart(document.getElementById('lineChart'), {
  type: 'line',
  data: {
    labels: weekDays,
    datasets: [
      { label:'Veld A', data:weekData.A, borderColor:'#ef4444', tension:0.4, borderWidth:2, pointRadius:3, fill:false },
      { label:'Veld B', data:weekData.B, borderColor:'#f59e0b', tension:0.4, borderWidth:2, pointRadius:3, fill:false, borderDash:[4,4] },
      { label:'Veld C', data:weekData.C, borderColor:'#3b6d11', tension:0.4, borderWidth:2, pointRadius:3, fill:false },
      { label:'Veld D', data:weekData.D, borderColor:'#b45309', tension:0.4, borderWidth:2, pointRadius:3, fill:false, borderDash:[6,3] },
      { label:'Veld E', data:weekData.E, borderColor:'#6aa84f', tension:0.4, borderWidth:2, pointRadius:3, fill:false },
      { label:'Veld F', data:weekData.F, borderColor:'#dc2626', tension:0.4, borderWidth:2, pointRadius:3, fill:false, borderDash:[2,4] },
    ]
  },
  options: {
    responsive: true, maintainAspectRatio: false,
    plugins: { legend: { display: false }, tooltip: { mode:'index', intersect:false } },
    scales: {
      x: { grid: { color:'#e8e8e2' }, ticks: { font:{size:11}, color:'#5a6e60' } },
      y: { grid: { color:'#e8e8e2' }, ticks: { font:{size:11}, color:'#5a6e60', callback: v => v + ' mm' }, min:30, max:125 }
    }
  }
});

// SCATTER
const scatterData = fields.map(f => ({ x:f.tof, y:f.sonic }));
new Chart(document.getElementById('scatterChart'), {
  type: 'scatter',
  data: {
    datasets: [
      {
        label: 'Sensor correlatie',
        data: scatterData,
        backgroundColor: fields.map(f => { const h=(f.tof+f.sonic)/2; return h>=90?'#ef4444':h>=65?'#f59e0b':'#3b6d11'; }),
        pointRadius: 8, pointHoverRadius: 10,
      },
      {
        label: 'Ideale lijn',
        data: [{x:30,y:30},{x:120,y:120}],
        type:'line', borderColor:'#e0e8d8', borderWidth:1.5, borderDash:[5,5], pointRadius:0, fill:false,
      }
    ]
  },
  options: {
    responsive: true, maintainAspectRatio: false,
    plugins: {
      legend: { display: false },
      tooltip: {
        callbacks: {
          label: ctx => {
            if (ctx.datasetIndex === 1) return null;
            const f = fields[ctx.dataIndex];
            return 'Veld ' + f.id + ' — TOF: ' + f.tof + ' mm / Sonic: ' + f.sonic + ' mm';
          }
        }
      }
    },
    scales: {
      x: { min:25, max:125, grid:{color:'#e8e8e2'}, title:{display:true,text:'TOF (mm)',color:'#5a6e60',font:{size:11}}, ticks:{font:{size:11},color:'#5a6e60',callback:v=>v+' mm'} },
      y: { min:25, max:125, grid:{color:'#e8e8e2'}, title:{display:true,text:'Sonic (mm)',color:'#5a6e60',font:{size:11}}, ticks:{font:{size:11},color:'#5a6e60',callback:v=>v+' mm'} },
    }
  }
});

// TABLE
const tbody = document.getElementById('detail-table');
const times = ['07:12','09:34','11:05','12:48','14:22','15:51'];
fields.forEach((f, i) => {
  const avg = Math.round((f.tof + f.sonic) / 2);
  const st = getStatus(avg);
  const pct = Math.round(avg / 120 * 100);
  const tr = document.createElement('tr');
  tr.className = 'border-b border-[#e0e8d8] last:border-0 hover:bg-[#f2fae8]';
  tr.innerHTML = `
    <td class="px-3.5 py-[11px] text-xs font-mono">${times[i]}</td>
    <td class="px-3.5 py-[11px] text-xs"><strong>Veld ${f.id}</strong> <span class="text-[#5a6e60] text-[11px]">${f.name}</span></td>
    <td class="px-3.5 py-[11px] text-xs font-mono">${f.tof}</td>
    <td class="px-3.5 py-[11px] text-xs font-mono">${f.sonic}</td>
    <td class="px-3.5 py-[11px] text-xs">
      <div class="flex items-center gap-2">
        <span class="font-mono text-xs min-w-[36px]">${avg}</span>
        <div class="flex-1 h-1.5 bg-[#e0e8d8] rounded-full overflow-hidden height-bar-track">
          <div class="h-full rounded-full transition-all duration-500" style="width:${pct}%;background:${st.fillColor};"></div>
        </div>
      </div>
    </td>
    <td class="px-3.5 py-[11px] text-xs font-mono">+${f.growth} mm</td>
    <td class="px-3.5 py-[11px] text-xs">
      <span class="inline-block px-[9px] py-[3px] rounded-full text-[10px] font-bold ${st.badgeCls}">${st.label}</span>
    </td>
  `;
  tbody.appendChild(tr);
});

// LOCATION LIST
const locList = document.getElementById('loc-list');
const pinColors = { 'Maaien':'#ef4444', 'Let op':'#f59e0b', 'Goed':'#3b6d11' };
fields.forEach(f => {
  const avg = Math.round((f.tof + f.sonic) / 2);
  const st = getStatus(avg);
  const el = document.createElement('div');
  el.className = 'flex items-center gap-2 px-3 py-2 bg-white border border-[#e0e8d8] rounded-lg text-[11px]';
  el.innerHTML = `
    <span class="w-2.5 h-2.5 rounded-full flex-shrink-0" style="background:${pinColors[st.label]};"></span>
    <span class="font-semibold text-ink">Veld ${f.id} — ${f.name}</span>
    <span class="inline-block px-[9px] py-[3px] rounded-full text-[10px] font-bold ml-2 ${st.badgeCls}">${avg} mm</span>
    <span class="ml-auto font-mono text-[10px] text-muted">${f.lat.toFixed(3)}°N ${f.lng.toFixed(3)}°O</span>
  `;
  locList.appendChild(el);
});

// SVG MAP
const mapWrap = document.getElementById('map-svg-wrap');
const mapW = mapWrap.offsetWidth || 460;
const mapH = 320;
const svgNS = 'http://www.w3.org/2000/svg';
const svg = document.createElementNS(svgNS, 'svg');
svg.setAttribute('viewBox', `0 0 ${mapW} ${mapH}`);
svg.setAttribute('width', '100%');
svg.setAttribute('height', mapH);
svg.style.background = '#d4e8c2';

const bg = document.createElementNS(svgNS, 'rect');
bg.setAttribute('width', mapW); bg.setAttribute('height', mapH); bg.setAttribute('fill', '#d4e8c2');
svg.appendChild(bg);

for (let i = 0; i < mapW; i += 30) {
  const l = document.createElementNS(svgNS, 'line');
  l.setAttribute('x1',i); l.setAttribute('y1',0); l.setAttribute('x2',i); l.setAttribute('y2',mapH);
  l.setAttribute('stroke','rgba(59,109,17,0.08)'); l.setAttribute('stroke-width','1');
  svg.appendChild(l);
}
for (let j = 0; j < mapH; j += 30) {
  const l = document.createElementNS(svgNS, 'line');
  l.setAttribute('x1',0); l.setAttribute('y1',j); l.setAttribute('x2',mapW); l.setAttribute('y2',j);
  l.setAttribute('stroke','rgba(59,109,17,0.08)'); l.setAttribute('stroke-width','1');
  svg.appendChild(l);
}

function project(lat, lng) {
  const latMin=51.430,latMax=51.447,lngMin=6.072,lngMax=6.092;
  return {
    x: ((lng-lngMin)/(lngMax-lngMin))*(mapW-80)+40,
    y: ((latMax-lat)/(latMax-latMin))*(mapH-80)+40
  };
}

fields.forEach(f => {
  const avg = Math.round((f.tof+f.sonic)/2);
  const st = getStatus(avg);
  const {x,y} = project(f.lat, f.lng);
  const color = pinColors[st.label];
  const ellipse = document.createElementNS(svgNS,'ellipse');
  ellipse.setAttribute('cx',x); ellipse.setAttribute('cy',y);
  ellipse.setAttribute('rx',22); ellipse.setAttribute('ry',14);
  ellipse.setAttribute('fill',color); ellipse.setAttribute('fill-opacity','0.18');
  ellipse.setAttribute('stroke',color); ellipse.setAttribute('stroke-width','1.5');
  svg.appendChild(ellipse);
});

fields.forEach(f => {
  const avg = Math.round((f.tof+f.sonic)/2);
  const st = getStatus(avg);
  const {x,y} = project(f.lat, f.lng);
  const color = pinColors[st.label];

  const pin = document.createElementNS(svgNS,'path');
  pin.setAttribute('d',`M${x},${y-4} Q${x+12},${y-18} ${x},${y-26} Q${x-12},${y-18} ${x},${y-4}Z`);
  pin.setAttribute('fill',color);
  svg.appendChild(pin);

  const circ = document.createElementNS(svgNS,'circle');
  circ.setAttribute('cx',x); circ.setAttribute('cy',y-20); circ.setAttribute('r',9); circ.setAttribute('fill','white');
  svg.appendChild(circ);

  const lbl = document.createElementNS(svgNS,'text');
  lbl.setAttribute('x',x); lbl.setAttribute('y',y-16); lbl.setAttribute('text-anchor','middle');
  lbl.setAttribute('font-family','Plus Jakarta Sans,sans-serif'); lbl.setAttribute('font-size','8');
  lbl.setAttribute('font-weight','700'); lbl.setAttribute('fill',color);
  lbl.textContent = f.id;
  svg.appendChild(lbl);

  const nameLbl = document.createElementNS(svgNS,'text');
  nameLbl.setAttribute('x',x); nameLbl.setAttribute('y',y+8); nameLbl.setAttribute('text-anchor','middle');
  nameLbl.setAttribute('font-family','Plus Jakarta Sans,sans-serif'); nameLbl.setAttribute('font-size','7.5');
  nameLbl.setAttribute('font-weight','600'); nameLbl.setAttribute('fill','#1a2e1f');
  nameLbl.textContent = f.name.split(' ').slice(0,2).join(' ');
  svg.appendChild(nameLbl);
});

const waterLabel = document.createElementNS(svgNS,'text');
waterLabel.setAttribute('x',mapW-12); waterLabel.setAttribute('y',mapH-12); waterLabel.setAttribute('text-anchor','end');
waterLabel.setAttribute('font-family','Plus Jakarta Sans,sans-serif'); waterLabel.setAttribute('font-size','9');
waterLabel.setAttribute('fill','rgba(26,46,31,0.4)');
waterLabel.textContent = 'Groenwerf — Regio Limburg (indicatief)';
svg.appendChild(waterLabel);

mapWrap.appendChild(svg);