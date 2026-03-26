#pragma once
#include <cstdint>

class VibrationProcessor {
public:
    VibrationProcessor();

    // call every new accelerometer sample
    void updateAccel(float accel_z, float dt_s);

    // get vibration-corrected sonic reading
    uint16_t compensate(uint16_t sonic_mm) const;

    void reset();

private:
    static constexpr float HPF_ALPHA            = 0.95f; // high-pass filter coefficient, higher = passes lower freqs
    static constexpr float REJECT_THRESHOLD_G   = 0.3f; // reject sonic sample if accel spike exceeds this

    float _prev_raw         = 0.0f; // previous raw accel_z, for HPF
    float _prev_hpf         = 0.0f; // previous HPF output
    float _velocity         = 0.0f; // integrated velocity (m/s)
    float _displacement_mm  = 0.0f; // integrated displacement (mm)

    float _accel_z_filtered = 0.0f; // last HPF output, for rejection check
};