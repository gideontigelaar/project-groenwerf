(function () {
  const canvas = document.getElementById("pieChart");
  if (!canvas) return;

  // Size canvas to its container
  const container = document.getElementById("ChartContainer");
  const size = Math.min(container.clientWidth, 400);
  canvas.width = size;
  canvas.height = size;

  const ctx = canvas.getContext("2d");

  const qualityLevels = ["S", "A", "B", "C", "D"];
  const qualityLevel = "B";
  const percentages = [20, 20, 20, 20, 20];
  const colors = ["#16a34a", "#4ade80", "#a3e635", "#fbbf24", "#f97316"];
  const targetIndex = qualityLevels.indexOf(qualityLevel);
  const abovePct = percentages
    .slice(0, targetIndex + 1)
    .reduce((s, v) => s + v, 0);

  const cx = size / 2;
  const cy = size / 2;
  const outerR = size * 0.42;
  const innerR = size * 0.26;

  // Draw doughnut slices
  let startAngle = -Math.PI / 2;
  percentages.forEach((pct, i) => {
    const slice = (pct / 100) * 2 * Math.PI;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, outerR, startAngle, startAngle + slice);
    ctx.closePath();
    ctx.fillStyle = colors[i];
    ctx.fill();
    startAngle += slice;
  });

  // Punch out inner circle (white donut hole)
  ctx.beginPath();
  ctx.arc(cx, cy, innerR, 0, 2 * Math.PI);
  ctx.fillStyle = "#ffffff";
  ctx.fill();

  // Center label: quality level
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillStyle = colors[targetIndex];
  ctx.font = `bold ${Math.round(size * 0.1)}px sans-serif`;
  ctx.fillText(qualityLevel, cx, cy - size * 0.04);

  // Center label: percentage
  ctx.font = `bold ${Math.round(size * 0.08)}px sans-serif`;
  ctx.fillStyle = "#374151";
  ctx.fillText(abovePct + "%", cx, cy + size * 0.06);

  // Legend
  const legendY = size * 0.92;
  const legendStartX = size * 0.05;
  const boxSize = size * 0.04;
  const gap = size / qualityLevels.length;

  qualityLevels.forEach((label, i) => {
    const x = legendStartX + i * gap;
    ctx.fillStyle = colors[i];
    ctx.fillRect(x, legendY, boxSize, boxSize);
    ctx.fillStyle = "#374151";
    ctx.font = `${Math.round(size * 0.05)}px sans-serif`;
    ctx.textAlign = "left";
    ctx.textBaseline = "top";
    ctx.fillText(label, x + boxSize + 4, legendY);
  });
})();
