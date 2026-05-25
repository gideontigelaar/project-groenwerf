#pragma once

#include "sensors/tof_sensor.h"
#include "sensors/adxl345.h"
#include "sensors/rcwl1604.h"
#include "sensors/vma430_gps.h"
#include "sensors/tmp36.h"
#include "processing/sensor_processor.h"
#include "networkmanager.h"
#include <string>

// System LED states
enum class SystemState {
    CALIBRATING,         // Green and Yellow Solid
    READING,             // Green Solid
    TRANSMITTING,        // Flashing Green
    ERROR_WARNING,       // Yellow Solid
    WIFI_RECONNECTING,   // Flashing Yellow
    HALTED               // Green and Yellow Solid
};

class GrassMonitor {
public:
    GrassMonitor();

    void init();
    void run();

private:
    // for ToF (I2C0)
    static constexpr uint I2C0_SDA = 20;
    static constexpr uint I2C0_SCL = 21;
    static constexpr uint I2C_FREQ = 400000;

    // for ADXL345 (I2C1)
    static constexpr uint I2C1_SDA = 14;
    static constexpr uint I2C1_SCL = 15;

    // for RCWL-1604
    static constexpr uint RCWL_TRIG = 18;
    static constexpr uint RCWL_ECHO = 19;

    // for TMP36
    static constexpr uint TMP36_PIN = 28;

    // for Network LEDs
    static constexpr uint LED_GREEN  = 26;
    static constexpr uint LED_YELLOW = 22;

    // Timing constants
    static constexpr uint32_t TOF_INTERVAL_MS   = 20;
    static constexpr uint32_t SONIC_INTERVAL_MS = 50;
    static constexpr uint32_t ACCEL_INTERVAL_MS = 10;
    static constexpr uint32_t PRINT_INTERVAL_MS = 500;
    static constexpr uint32_t LOOP_TICK_MS      = 5;

    static constexpr int SEND_BATCH_SIZE = 10;

    // Core components
    TofSensor       _tof;
    ADXL345         _accel;
    RCWL1604        _ultrasonic;
    TMP36           _tmp36;
    SensorProcessor _processor;
    NetworkManager  _nm;

    // Status flags
    bool _tof_ok   = false;
    bool _accel_ok = false;
    bool _sonic_ok = false;
    bool _tmp36_ok = false;
    bool _gps_ok   = false;
    bool _using_tmp36 = false;

    // Timing state
    uint32_t _last_tof_ms   = 0;
    uint32_t _last_sonic_ms = 0;
    uint32_t _last_accel_ms = 0;
    uint32_t _last_print_ms = 0;

    // Data batch state
    int _reading_counter = 0;
    std::string _readings = "";

    // Internal methods
    void initSystemLeds();
    void updateSystemLeds(SystemState state);
    void scanI2c(i2c_inst_t* i2c, const char* label);
    void calibrateSensors();

    void pollNetwork();
    void pollSensors(uint32_t now_ms);
    void processAndSendBatch(uint32_t now_ms);
    void updateTemperature();
    std::string buildJsonReading();
};