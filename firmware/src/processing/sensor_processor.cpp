#include "processing/sensor_processor.h"
#include <algorithm>

void SensorProcessor::parseTof(uint16_t distance_mm) {
    _raw.tof_mm = distance_mm;
    _tof_median.push(static_cast<float>(distance_mm));
    if(!_cal.tof_calibrated) calibrate();
}

void SensorProcessor::parseSonic(uint16_t distance_mm) {
    _raw.sonic_mm = distance_mm;
    // push to all windows for instant switching
    _sonic_narrow.push(static_cast<float>(distance_mm));
    _sonic_wide.push(static_cast<float>(distance_mm));

    // continuous drift correction adapts to temp changes
    if (_cal.sonic_calibrated && _vibration.intensity() < Config::Sensor::VIBRATION_LOW_G) {
        if (_sonic_narrow.variance() < Config::Sensor::SONIC_VARIANCE_THRESHOLD) {
            float new_baseline = Config::Sensor::KNOWN_HEIGHT_MM - _sonic_narrow.get();
            // nudge baseline by 0.5% per idle sample
            _cal.sonic_offset_mm = (0.995f * _cal.sonic_offset_mm) + (0.005f * new_baseline);
        }
    }

    if(!_cal.sonic_calibrated) calibrate();
}

void SensorProcessor::parseAccel(float x, float y, float z, uint32_t now_ms) {
    _raw.accel_x = x;
    _raw.accel_y = y;
    _raw.accel_z = z;

    // calc deltatime in seconds
    float dt_s = (_last_accel_ms == 0) ? 0.0f : (now_ms - _last_accel_ms) / 1000.0f;
    _last_accel_ms = now_ms;

    if(dt_s > 0.0f && dt_s < 0.5f) {
        _vibration.updateAccel(z, dt_s);
    }
}

void SensorProcessor::calibrate() {
    // wait for enough samples to establish baseline
    if(!_cal.tof_calibrated && _tof_median.count() >= Config::Sensor::CALIBRATION_SAMPLES) {
        if (_tof_median.variance() < Config::Sensor::TOF_VARIANCE_THRESHOLD) {
            _cal.tof_offset_mm = Config::Sensor::KNOWN_HEIGHT_MM - _tof_median.get();
            _cal.tof_calibrated = true;
        }
    }

    if(!_cal.sonic_calibrated && _sonic_narrow.count() >= Config::Sensor::CALIBRATION_SAMPLES) {
        if (_sonic_narrow.variance() < Config::Sensor::SONIC_VARIANCE_THRESHOLD) {
            _cal.sonic_offset_mm = Config::Sensor::KNOWN_HEIGHT_MM - _sonic_narrow.get();
            _cal.sonic_calibrated = true;
        }
    }
}

uint16_t SensorProcessor::applyOffset(float median, float offset) const {
    float corrected = median + offset;
    // clamp result between 0 and ground height
    float grass = std::max(0.0f, std::min(Config::Sensor::KNOWN_HEIGHT_MM, Config::Sensor::KNOWN_HEIGHT_MM - corrected));
    return static_cast<uint16_t>(grass);
}

float SensorProcessor::activeSonicFilterValue() const {
    float g = _vibration.intensity();

    // blend narrow and wide windows smoothly
    if (g <= Config::Sensor::VIBRATION_LOW_G)  return _sonic_narrow.get();
    if (g >= Config::Sensor::VIBRATION_HIGH_G) return _sonic_wide.get();

    float t = (g - Config::Sensor::VIBRATION_LOW_G) / (Config::Sensor::VIBRATION_HIGH_G - Config::Sensor::VIBRATION_LOW_G);
    return ((1.0f - t) * _sonic_narrow.get()) + (t * _sonic_wide.get());
}

uint16_t SensorProcessor::grassHeightTof() const {
    if(!_cal.tof_calibrated) return 0;
    return applyOffset(_tof_median.get(), _cal.tof_offset_mm);
}

uint16_t SensorProcessor::grassHeightSonicMedian() const {
    if(!_cal.sonic_calibrated) return 0;
    // apply median filtering
    return applyOffset(activeSonicFilterValue(), _cal.sonic_offset_mm);
}

uint16_t SensorProcessor::grassHeightSonicAccel() const {
    if(!_cal.sonic_calibrated) return 0;
    // apply spike rejection to raw, then apply offset
    return applyOffset(_vibration.compensate(_raw.sonic_mm), _cal.sonic_offset_mm);
}

uint16_t SensorProcessor::grassHeightSonicMedianAccel() const {
    if(!_cal.sonic_calibrated) return 0;
    // apply median + spike rejection
    return _vibration.compensate(grassHeightSonicMedian());
}

void SensorProcessor::reset() {
    _raw = RawData();
    _cal = CalibrationData();
    _last_accel_ms = 0;
    _tof_median.reset();
    _sonic_narrow.reset();
    _sonic_wide.reset();
    _vibration.reset();
}