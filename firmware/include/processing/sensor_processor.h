#pragma once
#include <cstdint>
#include "processing/moving_average.h"
#include "processing/median_filter.h"

struct RawData {
    uint16_t tof_mm = 0;
    uint16_t sonic_mm = 0;

    float accel_x = 0.0f;
    float accel_y = 0.0f;
    float accel_z = 0.0f;
};

struct ProcessedData {
    uint16_t grass_height_tof_mm = 0;
    uint16_t grass_height_sonic_mm = 0;
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
    void parseAccel(float x, float y, float z);

    void calibrate();
    bool is_calibrated() const { return _cal.is_calibrated; }
    CalibrationData get_calibration() const { return _cal; }

    const RawData& raw() const { return _raw; }
    const ProcessedData processed() const;

    void reset();

private:

    CalibrationData _cal;
    static constexpr float KNOWN_HEIGHT_MM = 85.0f; // your 8.5cm mount height

    RawData _raw;

    MedianFilter _tof_median{5};
    MovingAverage _tof_avg{5};
    MedianFilter _sonic_median{10};
    MovingAverage _sonic_avg{10};
};