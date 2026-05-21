#include "sensors/tof_sensor.h"
#include "sensors/adxl345.h"
#include "sensors/rcwl1604.h"
#include "sensors/vma430_gps.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include <cstdio>
#include <string>
#include "processing/sensor_processor.h"
#include "networkmanager.h"

// for ToF & ADXL345
constexpr uint I2C0_SDA = 12;
constexpr uint I2C0_SCL = 13;
constexpr uint I2C_FREQ = 400000;

// for RCWL-1604
constexpr uint RCWL_TRIG = 14;
constexpr uint RCWL_ECHO = 15;

// for Network LEDs
constexpr uint LED_GREEN = 18;
constexpr uint LED_YELLOW = 19;

constexpr uint32_t TOF_INTERVAL_MS   = 20;
constexpr uint32_t SONIC_INTERVAL_MS = 50;
constexpr uint32_t ACCEL_INTERVAL_MS = 10;
constexpr uint32_t PRINT_INTERVAL_MS = 250;
constexpr int      SEND_BATCH_SIZE   = 5;

// Network & System LED States
enum class SystemState {
    CALIBRATING,         // Green and Yellow Solid
    READING,             // Green Solid
    TRANSMITTING,        // Flashing Green
    ERROR_WARNING,       // Yellow Solid
    WIFI_RECONNECTING,   // Flashing Yellow
    HALTED               // Green and Yellow Solid
};

// Init LED pins
void init_system_leds() {
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);

    gpio_init(LED_YELLOW);
    gpio_set_dir(LED_YELLOW, GPIO_OUT);

    // Start with LEDs off
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_YELLOW, 0);
}

void update_system_leds(SystemState state) {
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    static SystemState last_state = SystemState::READING;
    static uint32_t state_start_ms = 0;

    if (state != last_state) {
        state_start_ms = now_ms;
        last_state = state;
    }

    uint32_t elapsed = now_ms - state_start_ms;

    bool led_blink_start_off = (elapsed / 125) % 2 != 0;

    switch (state) {
        case SystemState::CALIBRATING:
        case SystemState::HALTED:
            gpio_put(LED_GREEN, 1);
            gpio_put(LED_YELLOW, 1);
            break;

        case SystemState::READING:
            gpio_put(LED_GREEN, 1);
            gpio_put(LED_YELLOW, 0);
            break;

        case SystemState::TRANSMITTING:
            gpio_put(LED_GREEN, led_blink_start_off ? 1 : 0);
            gpio_put(LED_YELLOW, 0);
            break;

        case SystemState::ERROR_WARNING:
            gpio_put(LED_GREEN, 0);
            gpio_put(LED_YELLOW, 1);
            break;

        case SystemState::WIFI_RECONNECTING:
            gpio_put(LED_GREEN, 0);
            gpio_put(LED_YELLOW, led_blink_start_off ? 1 : 0);
            break;
    }
}

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

static std::string build_json_reading(
    bool tof_ok, bool sonic_ok, bool accel_ok, bool gps_ok,
    const CalibrationData& cal,
    SensorProcessor& processor)
{
    std::string obj = "{";
    RawData raw = processor.raw();

    // --- Processed data ---
    if (tof_ok && cal.tof_calibrated) {
        obj += "\"grassHeightTof\":" + std::to_string(processor.grassHeightTof());
    } else {
        obj += "\"grassHeightTof\":null";
    }

    if (sonic_ok && cal.sonic_calibrated) {
        obj += ",\"grassHeightSonicMedian\":" + std::to_string(processor.grassHeightSonicMedian());
    } else {
        obj += ",\"grassHeightSonicMedian\":null";
    }

    // --- Raw data ---
    obj += ",\"sonic_raw_mm\":"  + (sonic_ok ? std::to_string(raw.sonic_mm) : "null");
    obj += ",\"tof_raw_mm\":"    + (tof_ok ? std::to_string(raw.tof_mm) : "null");
    obj += ",\"accel_raw_x\":"   + (accel_ok ? std::to_string(raw.accel_x) : "null");
    obj += ",\"accel_raw_y\":"   + (accel_ok ? std::to_string(raw.accel_y) : "null");
    obj += ",\"accel_raw_z\":"   + (accel_ok ? std::to_string(raw.accel_z) : "null");

    // --- GPS data ---
    if (gps_ok && utc_time.valid) {
        char ts[32];
        snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02dZ",
            utc_time.year, utc_time.month, utc_time.day,
            utc_time.hour, utc_time.minute, utc_time.second);
        char lat_str[32], lon_str[32];
        snprintf(lat_str, sizeof(lat_str), "%.7f", location.latitude);
        snprintf(lon_str, sizeof(lon_str), "%.7f", location.longitude);

        obj += ",\"measured_at\":\"" + std::string(ts) + "\"";
        obj += ",\"lat\":" + std::string(lat_str);
        obj += ",\"lon\":" + std::string(lon_str);
    } else {
        obj += ",\"measured_at\":null,\"lat\":null,\"lon\":null";
    }

    obj += "}";
    return obj;
}

int main() {
    stdio_init_all();

    // Init LEDs
    init_system_leds();

    // while (!stdio_usb_connected()) {
    //     sleep_ms(100);
    // }
    sleep_ms(100);

    printf("=== Grass Monitor Pico ===\n\n");

    // Wi-Fi connection loop
    NetworkManager nm;
    nm.ConnectInitial();

    // Init sensors
    i2c_init(i2c0, I2C_FREQ);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);

    i2c_scan(i2c0, "i2c0 (GP12/GP13)");

    TofSensor tof(i2c0, TofSensor::DEFAULT_ADDR);
    ADXL345   accel(i2c0, ADXL345::DEFAULT_ADDR);
    RCWL1604  ultrasonic(RCWL_TRIG, RCWL_ECHO);

    bool tof_ok   = tof.init();
    bool accel_ok = accel.init();
    bool sonic_ok = ultrasonic.init();

    if (tof_ok) tof.startContinuous(TOF_INTERVAL_MS);

    // Calibration phase
    SensorProcessor processor;
    printf("\nCalibrating sensors based on ground height (keep the mower still)...\n");

    int cal_samples = 0;
    while(cal_samples < 25) {
        // Light up both LEDs
        update_system_leds(SystemState::CALIBRATING);

        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (tof_ok && tof.dataReady()) processor.parseTof(tof.readDistance());
        if (sonic_ok && (now_ms % 50 == 0)) processor.parseSonic(ultrasonic.readDistance());

        sleep_ms(20);
        cal_samples++;
    }
    processor.calibrate();
    printf("Calibration complete.\n\n");

    // Init GPS in background
    bool gps_ok = gps_init();
    if (!gps_ok) printf("GPS: Waiting for connection in background...\n\n");

    uint32_t last_tof_ms   = 0;
    uint32_t last_sonic_ms = 0;
    uint32_t last_accel_ms = 0;
    uint32_t last_print_ms = 0;

    int readingCounter = 0;
    std::string readings = "";

    while (true) {
        nm.Poll();
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        // Update LED state
        SystemState current_state = SystemState::READING;
        int wifi_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

        if (nm.IsHalted()) {
            current_state = SystemState::HALTED;
        } else if (wifi_status != CYW43_LINK_UP) {
            current_state = SystemState::WIFI_RECONNECTING;
        } else if (nm.HasError()) {
            current_state = SystemState::ERROR_WARNING;
        } else if (nm.IsBusy()) {
            current_state = SystemState::TRANSMITTING;
        }

        update_system_leds(current_state);

        // If halted, stop running sensors
        if (current_state == SystemState::HALTED) {
            sleep_ms(100);
            continue;
        }

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

        // Accel
        if (accel_ok && (now_ms - last_accel_ms) >= ACCEL_INTERVAL_MS) {
            AccelData accel_data = accel.read();
            processor.parseAccel(accel_data.x, accel_data.y, accel_data.z, now_ms);
            last_accel_ms = now_ms;
        }

        // GPS
        if (gps_ok) {
            if (gps_get_ubx_packet()) gps_parse_ubx_data();
        }

        // Print + accumulate batch
        if ((now_ms - last_print_ms) >= PRINT_INTERVAL_MS) {
            readingCounter++;
            RawData raw = processor.raw();

            printf("--- Reading (%d/%d) ---\n", readingCounter, SEND_BATCH_SIZE);
            printf("  Raw:  ToF:%4d mm | Sonic:%4d mm | Accel: X:%.2f Y:%.2f Z:%.2f\n",
                raw.tof_mm, raw.sonic_mm, raw.accel_x, raw.accel_y, raw.accel_z);
            printf("  Proc: ToF:%4d mm | SonicMed:%4d mm | SonicAcc:%4d mm\n",
                processor.grassHeightTof(), processor.grassHeightSonicMedian(), processor.grassHeightSonicAccel());

            if (gps_ok && utc_time.valid) {
                printf("  GPS:  %04d-%02d-%02d %02d:%02d:%02d | Lat:%.6f Lon:%.6f\n",
                    utc_time.year, utc_time.month, utc_time.day,
                    utc_time.hour, utc_time.minute, utc_time.second,
                    location.latitude, location.longitude);
            } else {
                printf("  GPS:  Searching / No Fix\n");
            }
            printf("\n");

            last_print_ms = now_ms;

            // Accumulate data
            if (readingCounter > 1) readings += ",";
            readings += build_json_reading(tof_ok, sonic_ok, accel_ok, gps_ok, processor.get_calibration(), processor);

            // Transmit batch
            if (readingCounter >= SEND_BATCH_SIZE) {
                if (!nm.IsBusy()) {
                    std::string payload = "[" + readings + "]";
                    if (nm.StartSend(payload.c_str())) {
                        readings = "";
                        readingCounter = 0;
                    }
                } else {
                    printf("NetworkManager is busy, buffering readings...\n\n");
                    // Wait for next loop if still busy, keep counter at SEND_BATCH_SIZE
                    readingCounter--;
                }
            }
        }

        sleep_ms(1);
    }
}