#include "processing/sensor_processor.h"
#include <algorithm>

// Process raw tof sensor data
void SensorProcessor::parseTof(uint16_t distance_mm) {
    _raw.tof_mm = distance_mm;
    _tof_median.push(static_cast<float>(distance_mm));

    if (!_cal.is_calibrated) calibrate();
}

// Process raw sonic sensor data
void SensorProcessor::parseSonic(uint16_t distance_mm) {
    _raw.sonic_mm = distance_mm;
    _sonic_median.push(static_cast<float>(distance_mm));

    if (!_cal.is_calibrated) calibrate();
}

void SensorProcessor::calibrate() {
    if (_tof_median.count() < 5 || _sonic_median.count() < 5) return;

    _cal.tof_offset_mm = KNOWN_HEIGHT_MM - _tof_median.get();
    _cal.sonic_offset_mm = KNOWN_HEIGHT_MM - _sonic_median.get();
    _cal.is_calibrated = true;
}

uint16_t SensorProcessor::grassHeightTof() const {
    if (!_cal.is_calibrated) return 0;
    float corrected = _tof_median.get() + _cal.tof_offset_mm;
    float grass = std::min(std::max(KNOWN_HEIGHT_MM - corrected, 0.0f), KNOWN_HEIGHT_MM);
    return static_cast<uint16_t>(grass);
}

uint16_t SensorProcessor::grassHeightSonic() const {
    if (!_cal.is_calibrated) return 0;
    float corrected = _sonic_median.get() + _cal.sonic_offset_mm;
    float grass = std::min(std::max(KNOWN_HEIGHT_MM - corrected, 0.0f), KNOWN_HEIGHT_MM);
    return static_cast<uint16_t>(grass);
}

void SensorProcessor::reset() {
    _raw = RawData();
    _tof_median.reset();
    _sonic_median.reset();
    _cal = CalibrationData();
}