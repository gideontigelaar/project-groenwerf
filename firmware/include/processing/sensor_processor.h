#pragma once
#include <cstdint>
#include "processing/median_filter.h"
#include "processing/vibration_processor.h"

struct RawData {
    uint16_t tof_mm    = 0;
    uint16_t sonic_mm  = 0;
    float    accel_x   = 0.0f;
    float    accel_y   = 0.0f;
    float    accel_z   = 0.0f;
};

struct CalibrationData {
    float tof_offset_mm    = 0.0f;
    float sonic_offset_mm  = 0.0f;
    bool  tof_calibrated   = false;
    bool  sonic_calibrated = false;
};

class SensorProcessor {
public:
    void parseTof(uint16_t distance_mm);
    void parseSonic(uint16_t distance_mm);
    void parseAccel(float x, float y, float z, uint32_t now_ms);

    void calibrate();
    bool            is_calibrated()   const { return _cal.tof_calibrated && _cal.sonic_calibrated; }
    CalibrationData get_calibration() const { return _cal; }

    const RawData& raw() const { return _raw; }

    uint16_t grassHeightTof()               const;
    uint16_t grassHeightSonicMedian()       const; // sonic with median filter
    uint16_t grassHeightSonicAccel()        const; // sonic with accel filter
    uint16_t grassHeightSonicMedianAccel()  const; // sonic with accel + median filter

    // vibration intensity in g rms
    float vibrationIntensity() const { return _vibration.intensity(); }

    void reset();

private:
    static constexpr float  KNOWN_HEIGHT_MM     = 185.0f; // distance from sensor to ground
    static constexpr int    CALIBRATION_SAMPLES = 10;

    static constexpr float  VIBRATION_LOW_G     = 0.05f; // idle threshold
    static constexpr float  VIBRATION_HIGH_G    = 0.15f;
    static constexpr size_t WINDOW_NARROW       = 10; // fastest (500ms)
    static constexpr size_t WINDOW_MEDIUM       = 18;
    static constexpr size_t WINDOW_WIDE         = 25; // max smoothing has 1.25s latency

    CalibrationData _cal;
    RawData         _raw;

    MedianFilter        _tof_median{5};
    MedianFilter        _sonic_narrow{WINDOW_NARROW};
    MedianFilter        _sonic_medium{WINDOW_MEDIUM};
    MedianFilter        _sonic_wide{WINDOW_WIDE};
    VibrationProcessor  _vibration;

    uint32_t _last_accel_ms = 0;

    uint16_t applyOffset(float median, float offset) const;

    // returns reference to which median filter is appropriate for current vibration level
    const MedianFilter& activeSonicFilter() const;
};