#include "processing/sensor_processor.h"

// Process raw tof sensor data
void SensorProcessor::parseTof(uint16_t distance_mm) {
  _raw.tof_mm = distance_mm;

  float val = static_cast<float>(distance_mm);
  _tof_ma.push(val);
}

// Process raw sonic sensor data
void SensorProcessor::parseSonic(uint16_t distance_mm) {
  _raw.sonic_mm = distance_mm;

  float val = static_cast<float>(distance_mm);
  _sonic_ma.push(val);
}

// Process raw accelerometer data
void SensorProcessor::parseAccel(float x, float y, float z) {
  _raw.accel_x = x;
  _raw.accel_y = y;
  _raw.accel_z = z;
}

const ProcessedData SensorProcessor::processed() {
  ProcessedData data;

  data.grass_height_tof_mm = _tof_ma.get();
  data.grass_height_sonic_mm = _sonic_ma.get();

  return data;
}

void SensorProcessor::reset() {
    _raw = RawData();
    _tof_ma.reset();
    _sonic_ma.reset();
}