const ctx = document.getElementById('grassChart').getContext('2d');

new Chart(ctx, {
    type: 'line',
    data: {
        labels: ['Meting 1','Meting 2','Meting 3','Meting 4','Meting 5','Meting 6','Meting 7'],
        datasets: [{
            label: 'Gras hoogte (cm)',
            data: data,
            borderColor: '#3b82f6',
            backgroundColor: 'rgba(59,130,246,0.2)',
            fill: true,
            tension: 0.4
        },
        {
            label: 'Threshold',
            data: Array(7).fill(10),
            borderColor: '#ef4444',
            borderDash: [5,5],
            fill: false
        }]
    },
    options: {
        responsive: true,
        plugins: {
            legend: {
                display: true
            }
        }
    }
});