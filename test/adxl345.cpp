#include "adxl345.h"
#include "pico/stdlib.h"
#include <cstdio>

// registers from datasheet
static constexpr uint8_t REG_DEVID = 0x00; // always reads 0xE5
static constexpr uint8_t REG_POWER_CTL = 0x2D; // write 0x08 to start measuring
static constexpr uint8_t REG_DATA_FORMAT = 0x31; // range + resolution
static constexpr uint8_t REG_DATAX0 = 0x32; // first of 6 data bytes (x, y, z)

ADXL345::ADXL345(i2c_inst_t* i2c, uint8_t addr)
    : _i2c(i2c), _addr(addr) {}

void ADXL345::writeReg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { reg, value };
    i2c_write_blocking(_i2c, _addr, buf, 2, false);
}

uint8_t ADXL345::readReg(uint8_t reg) {
    uint8_t val = 0;
    i2c_write_blocking(_i2c, _addr, &reg, 1, true); // keep bus held for followup read
    i2c_read_blocking(_i2c, _addr, &val, 1, false);
    return val;
}

void ADXL345::readRegs(uint8_t reg, uint8_t* buf, uint8_t len) {
    i2c_write_blocking(_i2c, _addr, &reg, 1, true);
    i2c_read_blocking(_i2c, _addr, buf, len, false);
}

bool ADXL345::init() {
    uint8_t id = readReg(REG_DEVID);
    printf("  [ADXL345] device ID: 0x%02X (expect 0xE5)\n", id);
    if (id != 0xE5) return false;

    // +/-2g range, full resolution
    writeReg(REG_DATA_FORMAT, 0x08);

    // wake up and start measuring
    writeReg(REG_POWER_CTL, 0x08);

    sleep_ms(10);
    return true;
}

AccelData ADXL345::read() {
    uint8_t buf[6];
    readRegs(REG_DATAX0, buf, 6);

    // reassemble two bytes per axis into one 16-bit number
    int16_t raw_x = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t raw_y = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t raw_z = (int16_t)(buf[4] | (buf[5] << 8));

    // multiply by scale factor to convert raw counts to g's
    constexpr float SCALE = 0.0039f;

    return { raw_x * SCALE, raw_y * SCALE, raw_z * SCALE };
}