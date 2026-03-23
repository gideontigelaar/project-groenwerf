#include "processing/sensor_processor.h"

void SensorProcessor::parseTof(uint16_t distance_mm) {
  _raw.tof_mm = distance_mm;
  _processed.grass_height_tof_mm = distance_mm;
}

void SensorProcessor::parseSonic(uint16_t distance_mm) {
  _raw.sonic_mm = distance_mm;
  _processed.grass_height_sonic_mm = distance_mm;
}

void SensorProcessor::parseAccel(float x, float y, float z) {
  _raw.accel_x = x;
  _raw.accel_y = y;
  _raw.accel_z = z;
}