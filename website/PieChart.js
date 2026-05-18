const canvas = document.getElementById('pieChart');
const container = document.getElementById('ChartContainer');
canvas.height = container.width;
const ctx = canvas.getContext('2d');
const qualityLevels = ['S','A','B','C','D'];
const qualityLevelField = 'B'
const percentages = [20,20,20,20,20];
const targetIndex = qualityLevels.indexOf(qualityLevelField);

// Sum all percentages at or above the target level
const abovePercentage = percentages.slice(0, targetIndex + 1).reduce((sum, val) => sum + val, 0);

const colors = ["#16a34a", "#4ade80", "#a3e635", "#fbbf24", "#f97316"];

const centerTextPlugin = {
    id: 'centerText',
    afterDraw(chart) {
        const { ctx, chartArea: { top, bottom, left, right } } = chart;
        const centerX = (left + right) / 2;
        const centerY = (top + bottom) / 2;

        ctx.save();
        ctx.font = 'bold 24px sans-serif';
        ctx.fillStyle = colors[targetIndex];
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(qualityLevelField, centerX, centerY-15); // ← your text here
        ctx.fillText(abovePercentage + '%', centerX, centerY+20); // ← your text here
        ctx.restore();
    }
};

const pieChart = new Chart(ctx, {
    type: 'doughnut',
    plugins: [centerTextPlugin],
    data: {
        labels: ['S','A','B','C','D'],
        datasets: [{
            backgroundColor: colors,
            data: percentages
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: {
                display: true
            }
        }
    }
});