async function getAddress(lat, lng) {
    if (!lat || !lng || lat === 'null' || lng === 'null') {
        return 'No location';
    }
    try {
        const res = await fetch(`/api/geocode?lat=${lat}&lng=${lng}`);
        const data = await res.json();
        return data.address;
    } catch {
        return 'Unknown';
    }
}
 
async function fetchData() {
    const sort = document.getElementById('sortSelect').value;
    const tbody = document.getElementById('dataTableBody');
 
    try {
        const res = await fetch(`/api/data?sort=${sort}`);
        const data = await res.json();
 
        if (!Array.isArray(data) || data.length === 0) {
            tbody.innerHTML = '<tr><td colspan="5" class="text-center text-muted">Database unavailable — no data to display</td></tr>';
            return;
        }
 
        const rows = await Promise.all(data.map(async row => {
        const address = await getAddress(row.latitude, row.longitude);
        const h = parseInt(row.tof_mm || row.sonic_median_mm || 0);
        const badge = h >= 400
            ? `<span class="inline-block px-2 py-0.5 rounded-full text-[10px] font-bold bg-[#fee2e2] text-[#991b1b]">Maaien</span>`
            : h >= 80
            ? `<span class="inline-block px-2 py-0.5 rounded-full text-[10px] font-bold bg-[#fef3c7] text-[#92400e]">Let op</span>`
            : `<span class="inline-block px-2 py-0.5 rounded-full text-[10px] font-bold bg-[#dff0c8] text-[#2a5238]">Goed</span>`;
        return `
            <tr class="transition-colors duration-100 hover:bg-[#f2fae8]">
                <td class="text-[12px] px-4 py-[9px] border-b border-[#e8e8e2] text-[#1a2e1f]">${row.tof_mm ?? '-'}</td>
                <td class="text-[12px] px-4 py-[9px] border-b border-[#e8e8e2] text-[#1a2e1f]">${row.sonic_median_mm ?? '-'}</td>
                <td class="text-[12px] px-4 py-[9px] border-b border-[#e8e8e2]">${badge}</td>
                <td class="text-[12px] px-4 py-[9px] border-b border-[#e8e8e2] font-mono text-[11px] text-[#5a6e60]">${address}</td>
                <td class="text-[12px] px-4 py-[9px] border-b border-[#e8e8e2] text-[#1a2e1f]">${row.measured_at ?? '-'}</td>
            </tr>
            `;
        }));
 
        tbody.innerHTML = rows.join('');
    } catch {
        tbody.innerHTML = '<tr><td colspan="5" class="text-center text-danger">Could not reach the server</td></tr>';
    }
}
 
fetchData();
setInterval(fetchData, 10000);