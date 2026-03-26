#pragma once
#include <cstdint>
#include "processing/median_filter.h"

struct RawData {
    uint16_t tof_mm    = 0;
    uint16_t sonic_mm  = 0;
    float    accel_x   = 0.0f;
    float    accel_y   = 0.0f;
    float    accel_z   = 0.0f;
};

struct CalibrationData {
    float tof_offset_mm   = 0.0f;
    float sonic_offset_mm = 0.0f;
    bool  is_calibrated   = false;
};

class SensorProcessor {
public:
    void parseTof(uint16_t distance_mm);
    void parseSonic(uint16_t distance_mm);
    void parseAccel(float x, float y, float z) {
        _raw.accel_x = x;
        _raw.accel_y = y;
        _raw.accel_z = z;
    }

    void calibrate();
    bool            is_calibrated()   const { return _cal.is_calibrated; }
    CalibrationData get_calibration() const { return _cal; }

    const RawData& raw() const { return _raw; }

    uint16_t grassHeightTof()   const;
    uint16_t grassHeightSonic() const;

    void reset();

private:
    static constexpr float KNOWN_HEIGHT_MM     = 185.0f;
    static constexpr int   CALIBRATION_SAMPLES = 10;

    CalibrationData _cal;
    RawData         _raw;

    MedianFilter _tof_median{5};
    MedianFilter _sonic_median{10};

    uint16_t applyOffset(float median, float offset) const;
};