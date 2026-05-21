<?php
header('Content-Type: application/json');
// Stop showing errors/warnings in the API output
ini_set('display_errors', 0);

// Log them to your server log instead so you can still read them
ini_set('log_errors', 1); 

// Make sure your script declares it is sending JSON
header('Content-Type: application/json');

$lat = $_GET['lat'] ?? '';
$lng = $_GET['lng'] ?? '';

if ($lat == "null" || $lng == "null")
{
    echo json_encode(['address' => 'Unknown']);
    exit;
}
if (!$lat || !$lng) {
    echo json_encode(['address' => 'Unknown']);
    exit;
}

$url = "https://nominatim.openstreetmap.org/reverse?lat={$lat}&lon={$lng}&format=json";

$ch = curl_init();
curl_setopt($ch, CURLOPT_URL, $url);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_USERAGENT, 'GroenWerfApp/1.0');
curl_setopt($ch, CURLOPT_TIMEOUT, 10);
$response = curl_exec($ch);
$error = curl_error($ch);
curl_close($ch);

if ($error) {
    echo json_encode(['address' => 'cURL error: ' . $error]);
    exit;
}

$data = json_decode($response, true);

if (isset($data['display_name'])) {
    echo json_encode(['address' => $data['display_name']]);
} else {
    echo json_encode(['address' => 'Address not found']);
}
?>


