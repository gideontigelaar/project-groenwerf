#include "processing/vibration_processor.h"
#include <cmath>

VibrationProcessor::VibrationProcessor() {}

void VibrationProcessor::updateAccel(float accel_z, float dt_s) {
    float accel_ms2 = accel_z * 9.81f;

    // remove drift from accelerometer
    float hpf = HPF_ALPHA * (_prev_hpf + accel_ms2 - _prev_raw);
    _prev_raw = accel_ms2;
    _prev_hpf = hpf;
    _accel_z_filtered = hpf;

    _velocity = (_velocity + (hpf * dt_s)) * 0.98f;
    _displacement_mm = (_displacement_mm + (_velocity * dt_s * 1000.0f)) * 0.90f; // TODO: 10% offset from real measurement, change to more reliable method to handle vibrations
}

uint16_t VibrationProcessor::compensate(uint16_t sonic_mm) const {
    // reject sample entirely on large spike
    if (fabsf(_accel_z_filtered) > (REJECT_THRESHOLD_G * 9.81f)) {
        return sonic_mm;
    }

    float corrected = static_cast<float>(sonic_mm) - _displacement_mm;
    if (corrected < 0.0f) corrected = 0.0f;
    return static_cast<uint16_t>(corrected);
}

void VibrationProcessor::reset() {
    _prev_raw = 0.0f;
    _prev_hpf = 0.0f;
    _velocity = 0.0f;
    _displacement_mm = 0.0f;
    _accel_z_filtered = 0.0f;
}