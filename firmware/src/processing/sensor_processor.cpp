#include "processing/sensor_processor.h"
#include <algorithm>

void SensorProcessor::parseTof(uint16_t distance_mm) {
    _raw.tof_mm = distance_mm;
    _tof_median.push(static_cast<float>(distance_mm));
    if (!_cal.is_calibrated) calibrate();
}

void SensorProcessor::parseSonic(uint16_t distance_mm) {
    _raw.sonic_mm = distance_mm;
    _sonic_median.push(static_cast<float>(distance_mm));
    if (!_cal.is_calibrated) calibrate();
}

void SensorProcessor::calibrate() {
    if (_tof_median.count()   < CALIBRATION_SAMPLES) return;
    if (_sonic_median.count() < CALIBRATION_SAMPLES) return;

    _cal.tof_offset_mm   = KNOWN_HEIGHT_MM - _tof_median.get();
    _cal.sonic_offset_mm = KNOWN_HEIGHT_MM - _sonic_median.get();
    _cal.is_calibrated   = true;
}

uint16_t SensorProcessor::applyOffset(float median, float offset) const {
    float corrected = median + offset;
    float grass = std::max(0.0f, std::min(KNOWN_HEIGHT_MM, KNOWN_HEIGHT_MM - corrected));
    return static_cast<uint16_t>(grass);
}

uint16_t SensorProcessor::grassHeightTof() const {
    if (!_cal.is_calibrated) return 0;
    return applyOffset(_tof_median.get(), _cal.tof_offset_mm);
}

uint16_t SensorProcessor::grassHeightSonic() const {
    if (!_cal.is_calibrated) return 0;
    return applyOffset(_sonic_median.get(), _cal.sonic_offset_mm);
}

void SensorProcessor::reset() {
    _raw = RawData();
    _cal = CalibrationData();
    _tof_median.reset();
    _sonic_median.reset();
}