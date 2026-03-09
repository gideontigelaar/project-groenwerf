#pragma once
#include "hardware/i2c.h"
#include <cstdint>

class VL53L1X {
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x29; // or 0x41 if XSHUT pulled high

    explicit VL53L1X(i2c_inst_t* i2c, uint8_t addr = 0x41);

    bool init();
    bool startContinuous(uint32_t period_ms = 50);
    void stopContinuous();
    uint16_t readDistance();   // returns distance in mm, 0 on error
    bool dataReady();
    uint8_t rangeStatus();

private:
    i2c_inst_t* _i2c;
    uint8_t _addr;

    void     writeReg(uint16_t reg, uint8_t value);
    void     writeReg16(uint16_t reg, uint16_t value);
    void     writeReg32(uint16_t reg, uint32_t value);
    uint8_t  readReg(uint16_t reg);
    uint16_t readReg16(uint16_t reg);
    uint32_t readReg32(uint16_t reg);
};