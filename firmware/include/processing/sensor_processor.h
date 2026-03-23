#pragma once
#include <cstdint>
#include "processing/moving_average.h"

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

class SensorProcessor {
public:

    void parseTof(uint16_t distance_mm);
    void parseSonic(uint16_t distance_mm);
    void parseAccel(float x, float y, float z);

    const RawData& raw() const { return _raw; }
    const ProcessedData processed() const;

    void reset();

private:
    RawData _raw;

    MovingAverage _tof_ma{5};
    MovingAverage _sonic_ma{10};
};