#include "vl53l1x.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>

// Official ST ULD default config blob, written starting at reg 0x002D
static const uint8_t DEFAULT_CONFIG[] = {
    0x12, 0x00, 0x00, 0x11, 0x02, 0x00, 0x02, 0x08,
    0x00, 0x08, 0x10, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0xff, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x20, 0x0b, 0x00, 0x00, 0x02, 0x0a, 0x21,
    0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0xc8,
    0x00, 0x00, 0x38, 0xff, 0x01, 0x00, 0x08, 0x00,
    0x00, 0x01, 0xcc, 0x0f, 0x01, 0xf1, 0x0d, 0x01,
    0x68, 0x00, 0x80, 0x08, 0xb8, 0x00, 0x00, 0x00,
    0x00, 0x0f, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x0f, 0x0d, 0x0e, 0x0e, 0x00,
    0x00, 0x02, 0xc7, 0xff, 0x9B, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00
};

VL53L1X::VL53L1X(i2c_inst_t* i2c, uint8_t addr)
    : _i2c(i2c), _addr(addr) {}

void VL53L1X::writeReg(uint16_t reg, uint8_t value) {
    uint8_t buf[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), value };
    i2c_write_blocking(_i2c, _addr, buf, 3, false);
}

void VL53L1X::writeReg16(uint16_t reg, uint16_t value) {
    uint8_t buf[4] = {
        (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
        (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)
    };
    i2c_write_blocking(_i2c, _addr, buf, 4, false);
}

void VL53L1X::writeReg32(uint16_t reg, uint32_t value) {
    uint8_t buf[6] = {
        (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
        (uint8_t)(value >> 24), (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),  (uint8_t)(value & 0xFF)
    };
    i2c_write_blocking(_i2c, _addr, buf, 6, false);
}

uint8_t VL53L1X::readReg(uint16_t reg) {
    uint8_t addr_buf[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    uint8_t val = 0;
    i2c_write_blocking(_i2c, _addr, addr_buf, 2, true);
    sleep_us(10);
    i2c_read_blocking(_i2c, _addr, &val, 1, false);
    return val;
}

uint16_t VL53L1X::readReg16(uint16_t reg) {
    uint8_t addr_buf[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    uint8_t buf[2] = {};
    i2c_write_blocking(_i2c, _addr, addr_buf, 2, true);
    sleep_us(10);
    i2c_read_blocking(_i2c, _addr, buf, 2, false);
    return (uint16_t)(buf[0] << 8 | buf[1]);
}

uint32_t VL53L1X::readReg32(uint16_t reg) {
    uint8_t addr_buf[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    uint8_t buf[4] = {};
    i2c_write_blocking(_i2c, _addr, addr_buf, 2, true);
    sleep_us(10);
    i2c_read_blocking(_i2c, _addr, buf, 4, false);
    return (uint32_t)(buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3]);
}

bool VL53L1X::init() {
    // Wait for boot (FIRMWARE_SYSTEM_STATUS = 0x00E5, need bit0=1)
    uint32_t timeout = 100;
    while((readReg(0x00E5) & 0x01) == 0) {
        if(--timeout == 0) {
            printf("  [init] boot timeout\n");
            return false;
        }
        sleep_ms(10);
    }

    // Verify model ID (0x010F = 0xEACC)
    uint16_t model = readReg16(0x010F);
    printf("  [init] model ID: 0x%04X\n", model);
    if(model != 0xEACC) return false;

    // Write default config blob to registers 0x002D..0x0087
    uint8_t buf[2 + sizeof(DEFAULT_CONFIG)];
    buf[0] = 0x00;
    buf[1] = 0x2D;
    memcpy(&buf[2], DEFAULT_CONFIG, sizeof(DEFAULT_CONFIG));
    i2c_write_blocking(_i2c, _addr, buf, sizeof(buf), false);
    sleep_ms(10);

    // Verify a few config bytes were written
    printf("  [init] reg 0x002D = 0x%02X (expect 0x12)\n", readReg(0x002D));
    printf("  [init] reg 0x002E = 0x%02X (expect 0x00)\n", readReg(0x002E));

    // Start one shot to trigger VHV calibration
    writeReg(0x0087, 0x40);
    sleep_ms(200);

    // Wait for data ready
    timeout = 200;
    while((readReg(0x0031) & 0x01) != 0) {
        if(--timeout == 0) {
            printf("  [init] VHV cal timeout\n");
            return false;
        }
        sleep_ms(10);
    }
    printf("  [init] VHV cal done\n");

    // Clear interrupt
    writeReg(0x0086, 0x01);
    writeReg(0x0087, 0x00); // stop

    // Tune VHV
    writeReg(0x0008, 0x09);
    writeReg(0x000B, 0x00);

    return true;
}

bool VL53L1X::startContinuous(uint32_t period_ms) {
    writeReg32(0x006C, period_ms * 1000); // intermeasurement in microseconds
    writeReg(0x0086, 0x01); // clear interrupt
    writeReg(0x0087, 0x40); // start ranging
    return true;
}

void VL53L1X::stopContinuous() {
    writeReg(0x0087, 0x80);
    sleep_ms(1);
    writeReg(0x0086, 0x01);
}

bool VL53L1X::dataReady() {
    return (readReg(0x0031) & 0x01) == 0;
}

uint8_t VL53L1X::rangeStatus() {
    return (readReg(0x0089) >> 3) & 0x1F;
}

uint16_t VL53L1X::readDistance() {
    uint16_t dist = readReg16(0x0096);
    writeReg(0x0086, 0x01); // clear interrupt after reading
    return dist;
}