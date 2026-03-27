#pragma once
#include <cstdint>

class VibrationProcessor {
public:
    VibrationProcessor() = default;

    // call every new accelerometer sample
    void updateAccel(float accel_z, float dt_s);

    // get vibration-corrected sonic reading
    uint16_t compensate(uint16_t sonic_mm) const;

    // vibration intensity in g rms
    float intensity() const { return _rms_g; }

    bool lastWasSpike() const { return _last_was_spike; }

    void reset();

private:
    static constexpr float HPF_ALPHA            = 0.95f; // high-pass filter
    static constexpr float RMS_ALPHA            = 0.995f;
    static constexpr float SPIKE_THRESHOLD_G    = 0.3f; // spike rejection threshold

    // filter state
    float _prev_raw         = 0.0f;
    float _prev_hpf         = 0.0f;
    float _accel_z_filtered = 0.0f;

    // rms calculation
    float _rms_accum = 0.0f;
    float _rms_g     = 0.0f;

    // spike rejection state
    mutable uint16_t _last_good_mm      = 0;
    mutable bool     _last_was_spike    = false;
};