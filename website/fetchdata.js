async function getAddress(lat, lng) {
    if (!lat || !lng || lat === 'null' || lng === 'null') {
        return 'No location';
    }
    const res = await fetch(`geocode.php?lat=${lat}&lng=${lng}`);
    const data = await res.json();
    console.log(data);
    return data.address;
}

async function fetchData() {
    const sort = document.getElementById('sortSelect').value;
    const res = await fetch(`data.php?sort=${sort}`);
    const data = await res.json();

    const tbody = document.getElementById('dataTableBody');

    if (data.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" class="text-center">No data found</td></tr>';
        return;
    }

    // Fetch all addresses in parallel
    const rows = await Promise.all(data.map(async row => {
        const address = await getAddress(row.latitude, row.longitude);
        return `
            <tr>
                <td>${row.tof_mm}</td>
                <td>${row.sonic_mm}</td>
                <td colspan="2">${address}</td>
                <td>${row.measured_at}</td>
            </tr>
        `;
    }));

    tbody.innerHTML = rows.join('');
}

fetchData();

setInterval(fetchData, 10000)