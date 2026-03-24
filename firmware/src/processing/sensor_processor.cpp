#include "processing/sensor_processor.h"
#include <algorithm>

// Process raw tof sensor data
void SensorProcessor::parseTof(uint16_t distance_mm) {
  _raw.tof_mm = distance_mm;

  float val = static_cast<float>(distance_mm);
  _tof_avg.push(val);
  _tof_median.push(val);

  if (!_cal.is_calibrated) {
    calibrate();
  }
}

// Process raw sonic sensor data
void SensorProcessor::parseSonic(uint16_t distance_mm) {
  _raw.sonic_mm = distance_mm;

  float val = static_cast<float>(distance_mm);
  _sonic_avg.push(val);
  _sonic_median.push(val);
  
  if (!_cal.is_calibrated) {
    calibrate();
  }
}

// Process raw accelerometer data
void SensorProcessor::parseAccel(float x, float y, float z) {
  _raw.accel_x = x;
  _raw.accel_y = y;
  _raw.accel_z = z;
}

void SensorProcessor::calibrate() {
  if (_tof_median.count() < 5 || _sonic_median.count() < 5) return;

  float tof_reading   = _tof_median.get();
  float sonic_reading = _sonic_median.get();

  _cal.tof_offset_mm   = KNOWN_HEIGHT_MM - tof_reading;
  _cal.sonic_offset_mm = KNOWN_HEIGHT_MM - sonic_reading;
  _cal.is_calibrated   = true;
}

const ProcessedData SensorProcessor::processed() const {
  ProcessedData data;

  // return zeroed data until calibrated
  if (!_cal.is_calibrated) return data;  

  float tof_corrected   = _tof_median.get() + _cal.tof_offset_mm;
  float sonic_corrected = _sonic_median.get() + _cal.sonic_offset_mm;

  float grass_tof   = std::min(std::max(KNOWN_HEIGHT_MM - tof_corrected,   0.0f), KNOWN_HEIGHT_MM);
  float grass_sonic = std::min(std::max(KNOWN_HEIGHT_MM - sonic_corrected, 0.0f), KNOWN_HEIGHT_MM);

  data.grass_height_tof_mm   = static_cast<uint16_t>(grass_tof);
  data.grass_height_sonic_mm = static_cast<uint16_t>(grass_sonic);

  return data;
}

void SensorProcessor::reset() {
  _raw = RawData();
  _tof_avg.reset();
  _sonic_avg.reset();
  _tof_median.reset();
  _sonic_median.reset();
  _cal = CalibrationData();
}