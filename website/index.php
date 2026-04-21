<?php include 'data.php'; ?>

<!DOCTYPE html>
<html>
<head>
    <title>Groenwerf dashboard</title>
    <link rel="stylesheet" href="style.css">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4"></script>
</head>
<body>

<div class="sidebar">
    <h2>Groenvoorziening</h2>
    <ul>
        <li>Dashboard</li>
        <li class="active">Rapport</li>
    </ul>
</div>

<div class="main">
    <h1>Rapport</h1>

    <div class="cards">
        <div class="card">
            <h3>Max hoogte voor het maaien</h3>
            <p><?php echo $grassHeight; ?> cm</p>
        </div>

        <div class="card">
            <h3>Beeldkwaliteit</h3>
            <p><?php echo $threshold; ?> cm</p>
        </div>

        <div class="card">
            <h3>Behaald</h3>
            <p style="color: <?php echo $color; ?>">
                <?php echo $status; ?>
            </p>
        </div>
    </div>

    <div class="chart-container">
        <h2>Metingen</h2>
        <canvas id="grassChart"></canvas>
    </div>
</div>
<script>
  const data = <?php echo json_encode($history); ?>;
</script>
<script src="script.js"></script>

</body>
</html>