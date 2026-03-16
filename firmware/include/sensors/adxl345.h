#pragma once
#include "hardware/i2c.h"
#include <cstdint>

struct AccelData {
    float x;
    float y;
    float z;
};

class ADXL345 {
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x53;

    explicit ADXL345(i2c_inst_t* i2c, uint8_t addr = DEFAULT_ADDR);

    bool init();
    AccelData read();

private:
    i2c_inst_t* _i2c;
    uint8_t _addr;

    void writeReg(uint8_t reg, uint8_t value);
    uint8_t readReg(uint8_t reg);
    void readRegs(uint8_t reg, uint8_t* buf, uint8_t len);
};