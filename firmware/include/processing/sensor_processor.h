#pragma once
#include <cstdint>
#include "processing/median_filter.h"
#include "processing/vibration_processor.h"
#include "config.h"

struct RawData {
    uint16_t tof_mm         = 0;
    uint16_t sonic_mm       = 0;
    float    accel_x        = 0.0f;
    float    accel_y        = 0.0f;
    float    accel_z        = 0.0f;
    float    temperature_c  = 0.0f;
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
    void setTemperature(float temp_c) { _raw.temperature_c = temp_c; }

    uint16_t grassHeightTof()               const;
    uint16_t grassHeightSonicMedian()       const; // sonic with median filter
    uint16_t grassHeightSonicAccel()        const; // sonic with accel filter
    uint16_t grassHeightSonicMedianAccel()  const; // sonic with accel + median filter

    // vibration intensity in g rms
    float vibrationIntensity() const { return _vibration.intensity(); }

    void reset();

private:
    CalibrationData _cal;
    RawData         _raw;

    MedianFilter        _tof_median{5};
    MedianFilter        _sonic_narrow{Config::Sensor::WINDOW_NARROW};
    MedianFilter        _sonic_medium{Config::Sensor::WINDOW_MEDIUM};
    MedianFilter        _sonic_wide{Config::Sensor::WINDOW_WIDE};
    VibrationProcessor  _vibration;

    uint32_t _last_accel_ms = 0;

    uint16_t applyOffset(float median, float offset) const;

    // returns reference to which median filter is appropriate for current vibration level
    const MedianFilter& activeSonicFilter() const;
};