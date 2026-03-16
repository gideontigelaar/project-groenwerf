#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/i2c.h"
#include "sensors/vl53l1x.h"
#include "sensors/adxl345.h"
#include "sensors/rcwl1604.h"
#include <cstdio>

// for VL53L1X & ADXL345
constexpr uint I2C0_SDA = 0;
constexpr uint I2C0_SCL = 1;
constexpr uint I2C_FREQ = 400000;

// for RCWL-1604
constexpr uint RCWL_TRIG = 2;
constexpr uint RCWL_ECHO = 3;

constexpr uint32_t SONIC_INTERVAL_MS = 50;

static void i2c_scan(i2c_inst_t* i2c, const char* label) {
    printf("Scanning %s...\n", label);
    for(uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy;
        if(i2c_read_blocking(i2c, addr, &dummy, 1, false) >= 0) {
            printf("  Device found at 0x%02X\n", addr);
        }
    }
    printf("\n");
}

int main() {
    stdio_init_all();

    // wait for USB serial connection
    while(!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(100);

    i2c_init(i2c0, I2C_FREQ);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);

    printf("=== Grass Monitor Pico ===\n\n");
    i2c_scan(i2c0, "i2c0 (GP0/GP1)");

    VL53L1X tof(i2c0, VL53L1X::DEFAULT_ADDR);
    ADXL345 accel(i2c0, ADXL345::DEFAULT_ADDR);
    RCWL1604 ultrasonic(RCWL_TRIG, RCWL_ECHO);

    if(!tof.init()) {
        printf("WARNING: VL53L1X init failed, continuing without it\n\n");
    } else {
        printf("VL53L1X initialised OK\n\n");
        tof.startContinuous(20);
    }

    if(!accel.init()) {
        printf("WARNING: ADXL345 init failed, continuing without it\n\n");
    } else {
        printf("ADXL345 initialised OK\n\n");
    }

    if(!ultrasonic.init()) {
        printf("WARNING: RCWL1604 init failed, continuing without it\n\n");
    } else {
        printf("RCWL1604 initialised OK\n\n");
    }

    uint16_t last_udist = 0;
    uint32_t last_sonic_ms = 0;

    while(true) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        // fire ultrasonic independent of ToF fire speed
        if((now_ms - last_sonic_ms) >= SONIC_INTERVAL_MS) {
            last_udist = ultrasonic.readDistance();
            last_sonic_ms = now_ms;
        }

        // print whenever the ToF has a new sample
        if(tof.dataReady()) {
            uint8_t status = tof.rangeStatus();
            uint16_t dist = tof.readDistance();
            AccelData a = accel.read();

            printf("Measurement:\n");
            if(status == 0 || status == 1) {
                printf("  ToF:   %u mm\n", dist);
            } else {
                printf("  ToF:   -- (status %u)\n", status);
            }
            if(last_udist > 0) {
                printf("  Sonic: %u mm\n", last_udist);
            } else {
                printf("  Sonic: --\n");
            }
            printf("  Accel: (X%6.2f) (Y%6.2f) (Z%6.2f) (g)\n", a.x, a.y, a.z);
            printf("\n");
        }

        sleep_ms(5);
    }
}