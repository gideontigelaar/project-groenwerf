#include "sensors/tof_sensor.h"
#include "sensors/adxl345.h"
#include "sensors/rcwl1604.h"
#include "sensors/vma430_gps.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <cstdio>
#include "processing/sensor_processor.h"
#include "networkmanager.h"
#include "lwip/netif.h"
#include <string>

// for ToF & ADXL345
constexpr uint I2C0_SDA = 12;
constexpr uint I2C0_SCL = 13;
constexpr uint I2C_FREQ = 400000;

// for RCWL-1604
constexpr uint RCWL_TRIG = 14;
constexpr uint RCWL_ECHO = 15;

constexpr uint32_t TOF_INTERVAL_MS   = 20;
constexpr uint32_t SONIC_INTERVAL_MS = 50;
constexpr uint32_t ACCEL_INTERVAL_MS = 10;
constexpr uint32_t GPS_INTERVAL_MS   = 1000;
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

    NetworkManager nm;
    nm.Init();

    int readingCounter = 0;
    std::string data = "";

    i2c_init(i2c0, I2C_FREQ);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);

    printf("=== Grass Monitor Pico ===\n\n");
    i2c_scan(i2c0, "i2c0 (GP12/GP13)");

    TofSensor tof(i2c0, TofSensor::DEFAULT_ADDR);
    ADXL345   accel(i2c0, ADXL345::DEFAULT_ADDR);
    RCWL1604  ultrasonic(RCWL_TRIG, RCWL_ECHO);

    bool tof_ok   = tof.init();
    bool accel_ok = accel.init();
    bool sonic_ok = ultrasonic.init();
    bool gps_ok   = gps_init();

    if (!tof_ok) {
        printf("WARNING: ToF init failed, continuing without it\n\n");
    } else {
        printf("ToF initialised OK\n\n");
        tof.startContinuous(TOF_INTERVAL_MS);
    }

    if (!accel_ok) {
        printf("WARNING: ADXL345 init failed, continuing without it\n\n");
    } else {
        printf("ADXL345 initialised OK\n\n");
    }

    if (!sonic_ok) {
        printf("WARNING: RCWL1604 init failed, continuing without it\n\n");
    } else {
        printf("RCWL1604 initialised OK\n\n");
    }

    if (!gps_ok) {
        printf("WARNING: GPS init failed, continuing without it\n\n");
    } else {
        printf("VMA430 GPS initialised OK\n\n");
    }

    uint32_t last_tof_ms   = 0;
    uint32_t last_sonic_ms = 0;
    uint32_t last_accel_ms = 0;
    uint32_t last_gps_ms   = 0;
    uint32_t last_print_ms = 0;

    SensorProcessor processor;

    while(true) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        // ToF
        if (tof_ok && (now_ms - last_tof_ms) >= TOF_INTERVAL_MS) {
            if (tof.dataReady()) {
                processor.parseTof(tof.readDistance());
                last_tof_ms = now_ms;
            }
        }

        // Sonic
        if ((now_ms - last_sonic_ms) >= SONIC_INTERVAL_MS) {
            if (sonic_ok) processor.parseSonic(ultrasonic.readDistance());
            last_sonic_ms = now_ms;
        }

        // Accelerometer
        if (accel_ok && (now_ms - last_accel_ms) >= ACCEL_INTERVAL_MS) {
            AccelData accel_data = accel.read();
            processor.parseAccel(accel_data.x, accel_data.y, accel_data.z, now_ms);
            last_accel_ms = now_ms;
        }

        // GPS
        if (gps_ok && (now_ms - last_gps_ms) >= GPS_INTERVAL_MS) {
            if (gps_get_ubx_packet()) {
                gps_parse_ubx_data();
            }
            last_gps_ms = now_ms;
        }

        // Print
        if ((now_ms - last_print_ms) >= PRINT_INTERVAL_MS) {

            CalibrationData cal = processor.get_calibration();
            printf("Calibrated (%.2f, %.2f)\n\n",
                cal.tof_calibrated ? cal.tof_offset_mm : -1.0f,
                cal.sonic_calibrated ? cal.sonic_offset_mm : -1.0f
            );

            RawData raw = processor.raw();
            printf("Raw:\n");

            if (tof_ok) {
                printf("  ToF: %u mm\n", raw.tof_mm);
            } else {
                printf("  ToF: [offline]\n");
            }

            if (sonic_ok) {
                printf("  Sonic: %u mm\n", raw.sonic_mm);
            } else {
                printf("  Sonic: [offline]\n");
            }

            if (accel_ok) {
                printf("  Accel: x(%.2f) y(%.2f) z(%.2f)\n\n", raw.accel_x, raw.accel_y, raw.accel_z);
            } else {
                printf("  Accel: [offline]\n\n");
            }

            data += "{";
            printf("Processed:\n");
            
            if (tof_ok) {
            // Print processed data
                printf("  ToF: %u mm\n", processor.grassHeightTof());
                if (cal.tof_calibrated) data += "\"grassHeightTof\":" + std::to_string(processor.grassHeightTof());
            } else {
                printf("  ToF: [offline]\n");
                data += "\"grassHeightTof\":offline";
            }

            if(sonic_ok) {
                if(accel_ok) {
                    printf("  Sonic (accel): %u mm\n", processor.grassHeightSonicCompensated());
                    if (cal.sonic_calibrated) data += ",\"grassHeightSonic\":" + std::to_string(processor.grassHeightSonic());
                } else {
                    printf("  Sonic (accel): [accel offline]\n");
                    data += ",\"grassHeightSonic\":offline";
                }
            }
            else {
                printf("  Sonic: [offline]\n");
                data += ",\"grassHeightSonic\":offline";
            }

            if (gps_ok) {
                if (utc_time.valid) {
                    printf("  GPS time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                        utc_time.year, utc_time.month, utc_time.day,
                        utc_time.hour, utc_time.minute, utc_time.second);
                    printf("  GPS pos:  lat=%.7f lon=%.7f\n",
                        location.latitude, location.longitude);
                } else {
                    printf("  GPS: waiting for fix...\n");
                }
            } else {
                printf("  GPS: [offline]\n");
            }

            data += "}";
            if (readingCounter == 10) 
            {
                nm.SendData(data.data());
                printf(data.c_str());
                data = "";
                readingCounter = 0;
            }
            printf("------------------------------\n\n");
            last_print_ms = now_ms;
            if (cal.tof_calibrated || cal.sonic_calibrated) readingCounter++;
        }

        sleep_ms(1);
    }
}