#pragma once
#include "hardware/i2c.h"
#include <cstdint>

enum class TofModel {
    UNKNOWN = 0,
    VL53L0X,
    VL53L1X
};

class TofSensor {
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x29;

    explicit TofSensor(i2c_inst_t* i2c, uint8_t addr = DEFAULT_ADDR);

    bool init();
    bool startContinuous(uint32_t period_ms = 50);
    void stopContinuous();
    uint16_t readDistance(); // returns distance in mm
    bool dataReady();
    uint8_t rangeStatus();

    TofModel model() const { return _model; }

private:
    i2c_inst_t* _i2c;
    uint8_t _addr;
    TofModel _model = TofModel::UNKNOWN;
    uint8_t _l0x_stop_variable = 0;

    // 8-bit, for VL53L0X
    void writeReg8_8(uint8_t reg, uint8_t value);
    uint8_t readReg8_8(uint8_t reg);
    uint16_t readReg8_16(uint8_t reg);

    // 16-bit, for VL53L1X
    void writeReg16_8(uint16_t reg, uint8_t value);
    void writeReg16_16(uint16_t reg, uint16_t value);
    void writeReg16_32(uint16_t reg, uint32_t value);
    uint8_t readReg16_8(uint16_t reg);
    uint16_t readReg16_16(uint16_t reg);
    uint32_t readReg16_32(uint16_t reg);

    bool initVL53L0X();
    bool initVL53L1X();
};