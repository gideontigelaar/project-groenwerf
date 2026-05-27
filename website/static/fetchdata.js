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
            return `
                <tr>
                    <td>${row.tof_mm ?? '-'}</td>
                    <td>${row.sonic_median_mm ?? '-'}</td>
                    <td colspan="2">${address}</td>
                    <td>${row.measured_at ?? '-'}</td>
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