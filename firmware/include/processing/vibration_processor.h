#pragma once
#include <cstdint>

class VibrationProcessor {
public:
    VibrationProcessor();

    uint16_t compensate(uint16_t sonic_mm, float accel_x, float accel_y, float accel_z);

private:

};