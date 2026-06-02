#pragma once
#include <cstdint>
#include "config.h"

class VibrationProcessor {
public:
    VibrationProcessor() = default;

    // call every new accelerometer sample
    void updateAccel(float accel_z, float dt_s);

    // get vibration-corrected sonic reading
    uint16_t compensate(uint16_t sonic_mm) const;

    // vibration intensity in g rms
    float intensity() const { return _rms_g; }

    void reset();

private:
    // filter state
    float _prev_raw         = 0.0f;
    float _prev_hpf         = 0.0f;
    float _accel_z_filtered = 0.0f;

    // rms calculation
    float _rms_accum = 0.0f;
    float _rms_g     = 0.0f;

    // spike rejection state
    mutable uint16_t _last_good_mm   = 0;
    mutable bool     _last_was_spike = false;
};