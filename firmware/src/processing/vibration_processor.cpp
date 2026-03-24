#include "processing/vibration_processor.h"

VibrationProcessor::VibrationProcessor() {
}

uint16_t VibrationProcessor::compensate(uint16_t sonic_mm, float accel_x, float accel_y, float accel_z) {
    return sonic_mm;
}