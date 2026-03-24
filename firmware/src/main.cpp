#include "sensors/vl53l1x.h"
#include "sensors/adxl345.h"
#include "sensors/rcwl1604.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <cstdio>
#include "processing/sensor_processor.h"

// for VL53L1X & ADXL345
constexpr uint I2C0_SDA = 0;
constexpr uint I2C0_SCL = 1;
constexpr uint I2C_FREQ = 400000;

// for RCWL-1604
constexpr uint RCWL_TRIG = 2;
constexpr uint RCWL_ECHO = 3;

constexpr uint32_t TOF_INTERVAL_MS = 20;
constexpr uint32_t SONIC_INTERVAL_MS = 50;
constexpr uint32_t PRINT_INTERVAL_MS = 500;

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

    bool tof_ok = tof.init();
    bool accel_ok = accel.init();
    bool sonic_ok = ultrasonic.init();

    if(!tof_ok) {
        printf("WARNING: VL53L1X init failed, continuing without it\n\n");
    } else {
        printf("VL53L1X initialised OK\n\n");
        tof.startContinuous(TOF_INTERVAL_MS);
    }

    if(!accel_ok) {
        printf("WARNING: ADXL345 init failed, continuing without it\n\n");
    } else {
        printf("ADXL345 initialised OK\n\n");
    }

    if(!sonic_ok) {
        printf("WARNING: RCWL1604 init failed, continuing without it\n\n");
    } else {
        printf("RCWL1604 initialised OK\n\n");
    }

    uint32_t last_sonic_ms = 0;
    uint32_t last_tof_ms = 0;
    uint32_t last_accel_ms = 0;
    uint32_t last_print_ms = 0;

    SensorProcessor processor = SensorProcessor();

    while(true) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        // tof
        if(tof_ok && (now_ms - last_tof_ms) >= TOF_INTERVAL_MS) {
            if(tof.dataReady()) {
                processor.parseTof(tof.readDistance());
                last_tof_ms = now_ms;
            }
        }

        // sonic
        if(sonic_ok && (now_ms - last_sonic_ms) >= SONIC_INTERVAL_MS) {
            processor.parseSonic(ultrasonic.readDistance());
            last_sonic_ms = now_ms;
        }

        // accelerometer
        if(accel_ok && (now_ms - last_accel_ms) >= PRINT_INTERVAL_MS) {
            processor.parseAccel(accel.read().x, accel.read().y, accel.read().z);
            last_accel_ms = now_ms;
        }

        if((now_ms - last_print_ms) >= PRINT_INTERVAL_MS) {

            printf(processor.is_calibrated() ? "Calibrated (%.2f, %.2f)\n" : "Not calibrated\n", processor.get_calibration().tof_offset_mm, processor.get_calibration().sonic_offset_mm);

            // Print raw data
            RawData raw = processor.raw();
            printf("Raw:\n");

            if(tof_ok) {
                printf("  ToF: %u mm\n", raw.tof_mm);
            } else {
                printf("  ToF: [offline]\n");
            }

            if(sonic_ok) {
                printf("  Sonic: %u mm\n", raw.sonic_mm);
            } else {
                printf("  Sonic: [offline]\n");
            }

            if(accel_ok) {
                printf("  Accel: x(%.2f) y(%.2f) z(%.2f)\n\n",
                    raw.accel_x,
                    raw.accel_y,
                    raw.accel_z);
            } else {
                printf("  Accel: [offline]\n\n");
            }

            // Print processed data (absolute processed data, so ready-to-use values)
            ProcessedData processed = processor.processed();

            printf("Processed:\n");
            if(tof_ok) {
                printf("  ToF: %u cm\n", processed.grass_height_tof_mm / 10);
            } else {
                printf("  ToF: [offline]\n");
            }

            if(sonic_ok) {
                printf("  Sonic: %u cm\n", processed.grass_height_sonic_mm / 10);
            } else {
                printf("  Sonic: [offline]\n\n");
            }

            printf("------------------------------\n\n");
            last_print_ms = now_ms;
        }

        sleep_ms(5);
    }
}