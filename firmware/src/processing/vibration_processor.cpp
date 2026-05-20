#include "processing/vibration_processor.h"
#include <cmath>

void VibrationProcessor::updateAccel(float accel_z, float dt_s) {
    // ### high-pass filter ###
    float accel_ms2 = accel_z * 9.81f; // convert g reading to m/s^2

    // cancel out DC offset caused by gravity and center to 0
    float hpf = HPF_ALPHA * (_prev_hpf + accel_ms2 - _prev_raw);
    _prev_raw = accel_ms2;
    _prev_hpf = hpf;
    _accel_z_filtered = hpf; // stored in m/s^2 for spike check

    // ### rms vibration intensity ###
    _rms_accum = RMS_ALPHA * _rms_accum + (1.0f - RMS_ALPHA) * (hpf * hpf); // square signal and apply exponential moving avg
    _rms_g = sqrtf(_rms_accum) / 9.81f; // convert to g units
}

uint16_t VibrationProcessor::compensate(uint16_t sonic_mm) const {
    // ### spike rejection ###
    if(fabsf(_accel_z_filtered) > (SPIKE_THRESHOLD_G * 9.81f)) {
        _last_was_spike = true; // spike event is occurring
        return (_last_good_mm > 0) ? _last_good_mm : sonic_mm; // return last clean sample, or current as fallback
    }
    _last_was_spike = false;

    // no spike: accept & store reading
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
}