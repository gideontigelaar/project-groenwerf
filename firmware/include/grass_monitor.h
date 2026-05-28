#pragma once

#include "sensors/tof_sensor.h"
#include "sensors/adxl345.h"
#include "sensors/rcwl1604.h"
#include "sensors/tmp36.h"
#include "sensors/vma430_gps.h"
#include "processing/sensor_processor.h"
#include "networkmanager.h"
#include "config.h"
#include <vector>

#pragma pack(push, 1)
struct BinaryReading {
    float lat;
    float lon;
    uint16_t ght;
    uint16_t ghs;
    float t;
    uint16_t sr;
    uint16_t tr;
    float ax;
    float ay;
    float az;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t valid_time;
};
#pragma pack(pop)

// system led states
enum class SystemState {
    CALIBRATING,         // green and yellow solid
    READING,             // green solid
    TRANSMITTING,        // flashing green
    WIFI_RECONNECTING,   // flashing yellow
    ERROR_WARNING,       // yellow solid
    HALTED               // green and yellow solid
};

class GrassMonitor {
public:
    GrassMonitor();
    void init();
    void run();

private:
    // core components
    TofSensor       _tof;
    ADXL345         _accel;
    RCWL1604        _ultrasonic;
    TMP36           _tmp36;

    SensorProcessor _processor;
    NetworkManager  _nm;

    // status flags
    bool _tof_ok    = false;
    bool _accel_ok  = false;
    bool _sonic_ok  = false;
    bool _tmp36_ok  = false;
    bool _using_tmp36   = false;
    bool _gps_ok        = false;

    // timing state
    uint32_t _last_tof_ms   = 0;
    uint32_t _last_accel_ms = 0;
    uint32_t _last_sonic_ms = 0;
    uint32_t _last_print_ms = 0;

    // data batch state
    int _reading_counter = 0;
    std::vector<BinaryReading> _readings;

    // internal methods
    void pollNetwork();
    void pollSensors(uint32_t now_ms);
    void updateTemperature();
    void processAndSendBatch(uint32_t now_ms);
    void initSystemLeds();
    void updateSystemLeds(SystemState state);
    void scanI2c(i2c_inst_t* i2c, const char* label);
    void calibrateSensors();
    void appendBinaryReading();
};