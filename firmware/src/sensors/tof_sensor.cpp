#include "sensors/tof_sensor.h"
#include "pico/stdlib.h"
#include <cstring>

// Official ST ULD default config blob for VL53L1X, written starting at reg 0x002D
static const uint8_t VL53L1X_DEFAULT_CONFIG[] = {
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

TofSensor::TofSensor(i2c_inst_t* i2c, uint8_t addr)
    : _i2c(i2c), _addr(addr) {}

// 8-bit register addressing for VL53L0X
void TofSensor::writeReg8_8(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { reg, value };
    i2c_write_blocking(_i2c, _addr, buf, 2, false);
}

uint8_t TofSensor::readReg8_8(uint8_t reg) {
    uint8_t val = 0;
    i2c_write_blocking(_i2c, _addr, &reg, 1, true);
    sleep_us(10);
    i2c_read_blocking(_i2c, _addr, &val, 1, false);
    return val;
}

uint16_t TofSensor::readReg8_16(uint8_t reg) {
    uint8_t buf[2] = {0, 0};
    i2c_write_blocking(_i2c, _addr, &reg, 1, true);
    sleep_us(10);
    i2c_read_blocking(_i2c, _addr, buf, 2, false);
    return (uint16_t)(buf[0] << 8 | buf[1]);
}

// 16-bit register addressing for VL53L1X
void TofSensor::writeReg16_8(uint16_t reg, uint8_t value) {
    uint8_t buf[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), value };
    i2c_write_blocking(_i2c, _addr, buf, 3, false);
}

void TofSensor::writeReg16_16(uint16_t reg, uint16_t value) {
    uint8_t buf[4] = {
        (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
        (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)
    };
    i2c_write_blocking(_i2c, _addr, buf, 4, false);
}

void TofSensor::writeReg16_32(uint16_t reg, uint32_t value) {
    uint8_t buf[6] = {
        (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
        (uint8_t)(value >> 24), (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),  (uint8_t)(value & 0xFF)
    };
    i2c_write_blocking(_i2c, _addr, buf, 6, false);
}

uint8_t TofSensor::readReg16_8(uint16_t reg) {
    uint8_t addr_buf[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    uint8_t val = 0;
    i2c_write_blocking(_i2c, _addr, addr_buf, 2, true);
    sleep_us(10);
    i2c_read_blocking(_i2c, _addr, &val, 1, false);
    return val;
}

uint16_t TofSensor::readReg16_16(uint16_t reg) {
    uint8_t addr_buf[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    uint8_t buf[2] = {0, 0};
    i2c_write_blocking(_i2c, _addr, addr_buf, 2, true);
    sleep_us(10);
    i2c_read_blocking(_i2c, _addr, buf, 2, false);
    return (uint16_t)(buf[0] << 8 | buf[1]);
}

uint32_t TofSensor::readReg16_32(uint16_t reg) {
    uint8_t addr_buf[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    uint8_t buf[4] = {0, 0, 0, 0};
    i2c_write_blocking(_i2c, _addr, addr_buf, 2, true);
    sleep_us(10);
    i2c_read_blocking(_i2c, _addr, buf, 4, false);
    return (uint32_t)(buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3]);
}

bool TofSensor::init() {
    // Attempt VL53L0X detection
    uint8_t l0x_id = readReg8_8(0xC0);
    if (l0x_id == 0xEE) {
        _model = TofModel::VL53L0X;
        return initVL53L0X();
    }

    // Fall back to VL53L1X detection
    uint16_t l1x_id = readReg16_16(0x010F);
    if (l1x_id == 0xEACC) {
        _model = TofModel::VL53L1X;
        return initVL53L1X();
    }

    return false;
}

bool TofSensor::initVL53L0X() {
    // Data init sequence
    writeReg8_8(0x88, 0x00);
    writeReg8_8(0x80, 0x01);
    writeReg8_8(0xFF, 0x01);
    writeReg8_8(0x00, 0x00);
    _l0x_stop_variable = readReg8_8(0x91);
    writeReg8_8(0x00, 0x01);
    writeReg8_8(0xFF, 0x00);
    writeReg8_8(0x80, 0x00);

    // Disable MSRC and TCC by default
    uint8_t config = readReg8_8(0x60);
    writeReg8_8(0x60, config | 0x12);

    // Enable interrupts on new data ready
    writeReg8_8(0x0A, 0x04); // SYSTEM_INTERRUPT_CONFIG_GPIO
    uint8_t gpio_cfg = readReg8_8(0x84); // GPIO_HV_MUX_ACTIVE_HIGH
    writeReg8_8(0x84, gpio_cfg & ~0x10); // Active low
    writeReg8_8(0x0B, 0x01); // SYSTEM_INTERRUPT_CLEAR

    return true;
}

bool TofSensor::initVL53L1X() {
    // Wait for boot
    uint32_t timeout = 100;
    while((readReg16_8(0x00E5) & 0x01) == 0) {
        if(--timeout == 0) {
            return false;
        }
        sleep_ms(10);
    }

    // Write default config blob
    uint8_t buf[2 + sizeof(VL53L1X_DEFAULT_CONFIG)];
    buf[0] = 0x00;
    buf[1] = 0x2D;
    memcpy(&buf[2], VL53L1X_DEFAULT_CONFIG, sizeof(VL53L1X_DEFAULT_CONFIG));
    i2c_write_blocking(_i2c, _addr, buf, sizeof(buf), false);
    sleep_ms(10);

    // Start one shot to trigger VHV calibration
    writeReg16_8(0x0087, 0x40);
    sleep_ms(200);

    timeout = 200;
    while((readReg16_8(0x0031) & 0x01) != 0) {
        if(--timeout == 0) {
            return false;
        }
        sleep_ms(10);
    }

    // Clear interrupt and stop
    writeReg16_8(0x0086, 0x01);
    writeReg16_8(0x0087, 0x00);

    // Tune VHV
    writeReg16_8(0x0008, 0x09);
    writeReg16_8(0x000B, 0x00);

    return true;
}

bool TofSensor::startContinuous(uint32_t period_ms) {
    if (_model == TofModel::VL53L1X) {
        writeReg16_32(0x006C, period_ms * 1000);
        writeReg16_8(0x0086, 0x01);
        writeReg16_8(0x0087, 0x40);
        return true;
    } else if (_model == TofModel::VL53L0X) {
        writeReg8_8(0x80, 0x01);
        writeReg8_8(0xFF, 0x01);
        writeReg8_8(0x91, _l0x_stop_variable);
        writeReg8_8(0x00, 0x01);
        writeReg8_8(0xFF, 0x00);
        writeReg8_8(0x80, 0x00);

        writeReg8_8(0x00, 0x02); // SYSRANGE_START
        return true;
    }
    return false;
}

void TofSensor::stopContinuous() {
    if (_model == TofModel::VL53L1X) {
        writeReg16_8(0x0087, 0x80);
        sleep_ms(1);
        writeReg16_8(0x0086, 0x01);
    } else if (_model == TofModel::VL53L0X) {
        writeReg8_8(0x00, 0x01);
        writeReg8_8(0xFF, 0x01);
        writeReg8_8(0x00, 0x00);
        writeReg8_8(0x91, 0x00);
        writeReg8_8(0x00, 0x01);
        writeReg8_8(0xFF, 0x00);
    }
}

bool TofSensor::dataReady() {
    if (_model == TofModel::VL53L1X) {
        return (readReg16_8(0x0031) & 0x01) == 0;
    } else if (_model == TofModel::VL53L0X) {
        return (readReg8_8(0x13) & 0x07) != 0; // RESULT_INTERRUPT_STATUS
    }
    return false;
}

uint8_t TofSensor::rangeStatus() {
    if (_model == TofModel::VL53L1X) {
        return (readReg16_8(0x0089) >> 3) & 0x1F;
    } else if (_model == TofModel::VL53L0X) {
        return (readReg8_8(0x14) >> 3) & 0x0F; // RESULT_RANGE_STATUS
    }
    return 0;
}

uint16_t TofSensor::readDistance() {
    if (_model == TofModel::VL53L1X) {
        uint16_t dist = readReg16_16(0x0096);
        writeReg16_8(0x0086, 0x01); // clear interrupt
        return dist;
    } else if (_model == TofModel::VL53L0X) {
        uint16_t dist = readReg8_16(0x1E); // RESULT_RANGE_STATUS + 10
        writeReg8_8(0x0B, 0x01); // SYSTEM_INTERRUPT_CLEAR
        return dist;
    }
    return 0;
}