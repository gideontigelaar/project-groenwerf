<?php
header('Content-Type: application/json');

$conn = new mysqli('', '', '', '');

if ($conn->connect_error) {
    http_response_code(500);
    echo json_encode([]);
    exit;
}

$result = $conn->query("SELECT tof_mm, sonic_mm, longitude, latitude, measured_at FROM sensor_readings ORDER BY measured_at DESC LIMIT 100");

$rows = [];
while ($row = $result->fetch_assoc()) {
    $rows[] = $row;
}

echo json_encode($rows);
$conn->close();
?>