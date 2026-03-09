#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/i2c.h"
#include "vl53l1x.hpp"
#include <cstdio>

// --- Pin config ---
constexpr uint I2C_SDA = 4;
constexpr uint I2C_SCL = 5;
constexpr uint I2C_FREQ = 400000;

int main() {
    stdio_init_all();

    // Wait for USB serial connection
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(100);

    i2c_init(i2c0, I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    printf("VL53L1X Demo - Raspberry Pi Pico 2\n");

    // Scan I2C bus to confirm device is visible
    printf("Scanning I2C bus...\n");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy;
        int result = i2c_read_blocking(i2c0, addr, &dummy, 1, false);
        if (result >= 0) {
            printf("  Device found at 0x%02X\n", addr);
        }
    }

    VL53L1X sensor(i2c0, 0x29);

    if (!sensor.init()) {
        printf("ERROR: VL53L1X init failed!\n");
        while (true) {}
    }

    printf("VL53L1X initialised OK\n");
    sensor.startContinuous(20);

    while (true) {
        if (sensor.dataReady()) {
            uint8_t status = sensor.rangeStatus();
            uint16_t dist = sensor.readDistance();

            if (status == 0 || status == 1) {
                printf("Distance: %u mm\n", dist);
            }
        }
        sleep_ms(1);
    }
}