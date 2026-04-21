<?php
// Threshold
$threshold = 10;

// Generate simulated historical data (last 7 readings)
$history = [];
for ($i = 0; $i < 7; $i++) {
    $history[] = rand(5, 15);
}

// Average value
$grassHeight = max($history);

$difference = abs($grassHeight-$threshold)/$threshold*100;
// Status
$status = $grassHeight > $threshold ? "Te hoog met {$difference}%" : "Gehaald met {$difference}%";
$color = $grassHeight > $threshold ? "#e74c3c" : "#2ecc71";
?>