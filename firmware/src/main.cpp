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
constexpr uint32_t PRINT_INTERVAL_MS = 250;
constexpr int      SEND_BATCH_SIZE   = 10;

static void i2c_scan(i2c_inst_t* i2c, const char* label) {
    printf("Scanning %s...\n", label);
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy;
        if (i2c_read_blocking(i2c, addr, &dummy, 1, false) >= 0) {
            printf("  Device found at 0x%02X\n", addr);
        }
    }
    printf("\n");
}

// Returns a single JSON object string for one reading.
// All uncalibrated / offline fields are emitted as null (valid JSON).
static std::string build_json_reading(
    bool tof_ok, bool sonic_ok, bool accel_ok, bool gps_ok,
    const CalibrationData& cal,
    SensorProcessor& processor)
{
    std::string obj = "{";

    // --- ToF ---
    if (tof_ok && cal.tof_calibrated) {
        obj += "\"grassHeightTof\":" + std::to_string(processor.grassHeightTof());
    } else {
        obj += "\"grassHeightTof\":null";
    }

    // --- Sonic variants ---
    if (sonic_ok && cal.sonic_calibrated) {
        obj += ",\"grassHeightSonicMedian\":"    + std::to_string(processor.grassHeightSonicMedian());
        if (accel_ok) {
            obj += ",\"grassHeightSonicAccel\":"      + std::to_string(processor.grassHeightSonicAccel());
            obj += ",\"grassHeightSonicMedianAccel\":" + std::to_string(processor.grassHeightSonicMedianAccel());
        } else {
            obj += ",\"grassHeightSonicAccel\":null";
            obj += ",\"grassHeightSonicMedianAccel\":null";
        }
    } else {
        obj += ",\"grassHeightSonicMedian\":null";
        obj += ",\"grassHeightSonicAccel\":null";
        obj += ",\"grassHeightSonicMedianAccel\":null";
    }

    // --- GPS ---
    if (gps_ok && utc_time.valid) {
        // ISO 8601 timestamp
        char ts[32];
        snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02dZ",
            utc_time.year, utc_time.month, utc_time.day,
            utc_time.hour, utc_time.minute, utc_time.second);
        char coords[64];
        snprintf(coords, sizeof(coords), "%.7f", location.latitude);
        std::string lat_str(coords);
        snprintf(coords, sizeof(coords), "%.7f", location.longitude);
        std::string lon_str(coords);

        obj += ",\"gpsTime\":\"" + std::string(ts) + "\"";
        obj += ",\"lat\":"  + lat_str;
        obj += ",\"lon\":"  + lon_str;
    } else {
        obj += ",\"gpsTime\":null";
        obj += ",\"lat\":null";
        obj += ",\"lon\":null";
    }

    obj += "}";
    return obj;
}

int main() {
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(100);

    NetworkManager nm;
    nm.Init();

    nm.Poll();

    if (nm.IsDone()) {
        printf("Send complete\n");
        nm.ResetState();
    }
    if (nm.HasError()) {
        printf("Send failed\n");
        nm.ResetState();
    }

    int         readingCounter = 0;
    std::string readings       = "";   // accumulates JSON objects

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
    uint32_t last_print_ms = 0;

    SensorProcessor processor;

    while (true) {
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
        if (gps_ok) {
            if (gps_get_ubx_packet()) {
                gps_parse_ubx_data();
            }
        }

        // Print + batch send
        if ((now_ms - last_print_ms) >= PRINT_INTERVAL_MS) {

            CalibrationData cal = processor.get_calibration();
            printf("Calibrated (%.2f, %.2f)\n\n",
                cal.tof_calibrated  ? cal.tof_offset_mm   : -1.0f,
                cal.sonic_calibrated ? cal.sonic_offset_mm : -1.0f
            );

            // --- Raw ---
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
                printf("  Accel: x(%.2f) y(%.2f) z(%.2f)\n\n",
                    raw.accel_x, raw.accel_y, raw.accel_z);
            } else {
                printf("  Accel: [offline]\n\n");
            }

            // --- Processed ---
            printf("Processed:\n");

            if (tof_ok) {
                printf("  ToF: %u mm\n", processor.grassHeightTof());
            } else {
                printf("  ToF: [offline]\n");
            }

            if (sonic_ok) {
                printf("  Sonic (median): %u mm\n", processor.grassHeightSonicMedian());
                if (accel_ok) {
                    printf("  Sonic (accel): %u mm\n",         processor.grassHeightSonicAccel());
                    printf("  Sonic (median+accel): %u mm\n",  processor.grassHeightSonicMedianAccel());
                } else {
                    printf("  Sonic (accel): [accel offline]\n");
                    printf("  Sonic (median+accel): [accel offline]\n");
                }
            } else {
                printf("  Sonic (median): [offline]\n");
                printf("  Sonic (accel): [offline]\n");
                printf("  Sonic (median+accel): [offline]\n");
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

            printf("------------------------------\n\n");
            last_print_ms = now_ms;

            // Accumulate into the JSON array if at least one sensor is calibrated
            if (cal.tof_calibrated || cal.sonic_calibrated) {
                if (readingCounter > 0) readings += ",";
                readings += build_json_reading(
                    tof_ok, sonic_ok, accel_ok, gps_ok, cal, processor);
                readingCounter++;
            }

            // Send when the batch is full
            if (readingCounter >= SEND_BATCH_SIZE) {
            std::string payload = "[" + readings + "]";
            printf("Sending: %s\n\n", payload.c_str());
            if (nm.StartSend(payload.c_str())) {
                readings       = "";
                readingCounter = 0;
            } else {
                printf("Failed to start send, will retry in next batch\n\n");
            }
        }
        }

        sleep_ms(1);
    }
}