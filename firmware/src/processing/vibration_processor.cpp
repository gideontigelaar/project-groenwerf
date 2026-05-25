#include "processing/vibration_processor.h"
#include <cmath>

void VibrationProcessor::updateAccel(float accel_z, float dt_s) {
    // high-pass filter
    float accel_ms2 = accel_z * 9.81f;

    // cancel out dc offset caused by gravity and center to 0
    float hpf = Config::Sensor::HPF_ALPHA * (_prev_hpf + accel_ms2 - _prev_raw);
    _prev_raw = accel_ms2;
    _prev_hpf = hpf;
    _accel_z_filtered = hpf;

    // rms vibration intensity
    _rms_accum = Config::Sensor::RMS_ALPHA * _rms_accum + (1.0f - Config::Sensor::RMS_ALPHA) * (hpf * hpf);
    _rms_g = sqrtf(_rms_accum) / 9.81f;
}

uint16_t VibrationProcessor::compensate(uint16_t sonic_mm) const {
    // spike rejection
    if(fabsf(_accel_z_filtered) > (Config::Sensor::SPIKE_THRESHOLD_G * 9.81f)) {
        _spike_hold_count++;

        // give up holding if spike lasts too long
        if (_spike_hold_count > Config::Sensor::MAX_SPIKE_HOLD) {
            _last_good_mm = sonic_mm;
            _spike_hold_count = 0;
            _last_was_spike = false;
            return sonic_mm;
        }

        _last_was_spike = true;
        return (_last_good_mm > 0) ? _last_good_mm : sonic_mm;
    }

    _last_was_spike = false;
    _spike_hold_count = 0;

    // accept & store reading
    _last_good_mm = sonic_mm;
    return sonic_mm;
}

void VibrationProcessor::reset() {
    _prev_raw = 0.0f;
    _prev_hpf = 0.0f;
    _accel_z_filtered = 0.0f;
    _rms_accum = 0.0f;
    _rms_g = 0.0f;
    _last_good_mm = 0;
    _last_was_spike = false;
    _spike_hold_count = 0;
}