#include "processing/sensor_processor.h"
#include <algorithm>

void SensorProcessor::parseTof(uint16_t distance_mm) {
    _raw.tof_mm = distance_mm;
    _tof_median.push(static_cast<float>(distance_mm));
    if (!_cal.tof_calibrated) calibrate();
}

void SensorProcessor::parseSonic(uint16_t distance_mm) {
    _raw.sonic_mm = distance_mm;
    _sonic_median.push(static_cast<float>(distance_mm));
    if (!_cal.sonic_calibrated) calibrate();
}

void SensorProcessor::parseAccel(float x, float y, float z, uint32_t now_ms) {
    _raw.accel_x = x;
    _raw.accel_y = y;
    _raw.accel_z = z;

    float dt_s = (_last_accel_ms == 0) ? 0.0f : (now_ms - _last_accel_ms) / 1000.0f;
    _last_accel_ms = now_ms;

    if (dt_s > 0.0f && dt_s < 0.5f) {
        _vibration.updateAccel(z, dt_s);
    }
}

void SensorProcessor::calibrate() {
    if (!_cal.tof_calibrated && _tof_median.count() >= CALIBRATION_SAMPLES) {
        _cal.tof_offset_mm = KNOWN_HEIGHT_MM - _tof_median.get();
        _cal.tof_calibrated = true;
    }

    if (!_cal.sonic_calibrated && _sonic_median.count() >= CALIBRATION_SAMPLES) {
        _cal.sonic_offset_mm = KNOWN_HEIGHT_MM - _sonic_median.get();
        _cal.sonic_calibrated = true;
    }
}

uint16_t SensorProcessor::applyOffset(float median, float offset) const {
    float corrected = median + offset;
    float grass = std::max(0.0f, std::min(KNOWN_HEIGHT_MM, KNOWN_HEIGHT_MM - corrected));
    return static_cast<uint16_t>(grass);
}

uint16_t SensorProcessor::grassHeightTof() const {
    if (!_cal.tof_calibrated) return 0;
    return applyOffset(_tof_median.get(), _cal.tof_offset_mm);
}

uint16_t SensorProcessor::grassHeightSonic() const {
    if (!_cal.sonic_calibrated) return 0;
    return applyOffset(_sonic_median.get(), _cal.sonic_offset_mm);
}

uint16_t SensorProcessor::grassHeightSonicCompensated() const {
    if (!_cal.sonic_calibrated) return 0;
    return _vibration.compensate(grassHeightSonic());
}

void SensorProcessor::reset() {
    _raw = RawData();
    _cal = CalibrationData();
    _last_accel_ms = 0;
    _tof_median.reset();
    _sonic_median.reset();
    _vibration.reset();
}