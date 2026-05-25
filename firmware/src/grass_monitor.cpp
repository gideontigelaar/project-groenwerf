#include "grass_monitor.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include <cstdio>

GrassMonitor::GrassMonitor()
    : _tof(i2c0, TofSensor::DEFAULT_ADDR),
      _accel(i2c1, ADXL345::DEFAULT_ADDR),
      _ultrasonic(RCWL_TRIG, RCWL_ECHO),
      _tmp36(TMP36_PIN)
{
}

void GrassMonitor::init() {
    stdio_init_all();

    // Init LEDs
    initSystemLeds();

    // Init ADC and internal temp sensor
    adc_init();
    adc_set_temp_sensor_enabled(true);

    sleep_ms(100);

    printf("=== Grass Monitor Pico ===\n\n");

    // Wi-Fi connection
    _nm.ConnectInitial();

    // Init I2C0
    i2c_init(i2c0, I2C_FREQ);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);

    // Init I2C1
    i2c_init(i2c1, I2C_FREQ);
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);

    scanI2c(i2c0, "i2c0 ToF (GP20/GP21)");
    scanI2c(i2c1, "i2c1 ADXL345 (GP14/GP15)");

    // Initialize Sensors
    _tof_ok   = _tof.init();
    _accel_ok = _accel.init();
    _sonic_ok = _ultrasonic.init();
    _tmp36_ok = _tmp36.init();

    if (!_tof_ok && !_sonic_ok) {
        printf("FATAL: Neither ToF nor Sonic sensor found — cannot measure grass height. Halting.\n");
        while (true) {
            updateSystemLeds(SystemState::ERROR_WARNING);
            sleep_ms(100);
        }
    }
    if (!_tof_ok)   printf("WARNING: ToF sensor not found — ToF grass height unavailable.\n");
    if (!_sonic_ok) printf("WARNING: Sonic sensor not found — Sonic grass height unavailable.\n");
    if (!_accel_ok) printf("WARNING: Accelerometer not found — vibration compensation unavailable.\n");

    // Disable if missing at boot
    if (_tmp36_ok && _tmp36.isConnected()) {
        printf("  [TMP36] Sensor found and reading valid temperature.\n");
    } else {
        printf("WARNING: TMP36 not found or disconnected — falling back to internal sensor.\n");
        _tmp36_ok = false;
    }

    if (_tof_ok) _tof.startContinuous(TOF_INTERVAL_MS);

    // Calibration phase
    calibrateSensors();

    // Init GPS in background
    _gps_ok = gps_init();
    if (!_gps_ok) printf("GPS: Waiting for connection in background...\n\n");
}

void GrassMonitor::run() {
    while (true) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        pollNetwork();

        // If halted, stop running sensors
        if (_nm.IsHalted()) {
            sleep_ms(100);
            continue;
        }

        pollSensors(now_ms);
        processAndSendBatch(now_ms);

        uint32_t loop_elapsed = to_ms_since_boot(get_absolute_time()) - now_ms;
        if (loop_elapsed < LOOP_TICK_MS) {
            sleep_ms(LOOP_TICK_MS - loop_elapsed);
        }
    }
}

void GrassMonitor::pollNetwork() {
    _nm.Poll();

    // Determine system state based on network and halting status
    SystemState current_state = SystemState::READING;
    int wifi_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (_nm.IsHalted()) {
        current_state = SystemState::HALTED;
    } else if (wifi_status != CYW43_LINK_UP) {
        current_state = SystemState::WIFI_RECONNECTING;
    } else if (_nm.HasError()) {
        current_state = SystemState::ERROR_WARNING;
    } else if (_nm.IsBusy()) {
        current_state = SystemState::TRANSMITTING;
    }

    updateSystemLeds(current_state);
}

void GrassMonitor::pollSensors(uint32_t now_ms) {
    // ToF
    if (_tof_ok && (now_ms - _last_tof_ms) >= TOF_INTERVAL_MS) {
        if (_tof.dataReady()) {
            _processor.parseTof(_tof.readDistance());
            _last_tof_ms = now_ms;
        }
    }

    // Sonic & Temp
    if ((now_ms - _last_sonic_ms) >= SONIC_INTERVAL_MS) {
        updateTemperature();

        if (_sonic_ok) {
            float current_temp_c = _processor.raw().temperature_c;
            _processor.parseSonic(_ultrasonic.readDistance(current_temp_c));
        }
        _last_sonic_ms = now_ms;
    }

    // Accel
    if (_accel_ok && (now_ms - _last_accel_ms) >= ACCEL_INTERVAL_MS) {
        AccelData accel_data = _accel.read();
        _processor.parseAccel(accel_data.x, accel_data.y, accel_data.z, now_ms);
        _last_accel_ms = now_ms;
    }

    // GPS
    if (_gps_ok) {
        if (gps_get_ubx_packet()) gps_parse_ubx_data();
    }
}

void GrassMonitor::updateTemperature() {
    float current_temp_c = 20.0f;
    _using_tmp36 = false;

    if (_tmp36_ok) {
        float t = _tmp36.readTemperature();
        // If temp sensor disconnects, lock out to prevent bad readings
        if (t >= -10.0f && t <= 60.0f) {
            current_temp_c = t;
            _using_tmp36 = true;
        } else {
            _tmp36_ok = false;
        }
    }

    if (!_using_tmp36) {
        // Fallback to internal temp sensor
        adc_select_input(4);
        const float conversion_factor = 3.3f / (1 << 12);
        float adc_voltage = (float)adc_read() * conversion_factor;
        // Subtract 5.0f for chip heat offset
        current_temp_c = 27.0f - (adc_voltage - 0.706f) / 0.001721f - 5.0f;
    }

    _processor.setTemperature(current_temp_c);
}

void GrassMonitor::processAndSendBatch(uint32_t now_ms) {
    if ((now_ms - _last_print_ms) < PRINT_INTERVAL_MS) {
        return;
    }

    _reading_counter++;
    RawData raw = _processor.raw();

    printf("--- Reading (%d/%d) ---\n", _reading_counter, SEND_BATCH_SIZE);
    printf("  Raw:  ToF:%4d mm | Sonic:%4d mm | Accel: X:%.2f Y:%.2f Z:%.2f\n",
        raw.tof_mm, raw.sonic_mm, raw.accel_x, raw.accel_y, raw.accel_z);
    printf("  Proc: ToF:%4d mm | SonicMed:%4d mm | SonicAcc:%4d mm | Temp:%.1f C (%s)\n",
        _processor.grassHeightTof(), _processor.grassHeightSonicMedian(), _processor.grassHeightSonicAccel(), raw.temperature_c,
        _using_tmp36 ? "TMP36" : "Internal");

    if (_gps_ok && utc_time.valid) {
        printf("  GPS:  %04d-%02d-%02d %02d:%02d:%02d | Lat:%.6f Lon:%.6f\n",
            utc_time.year, utc_time.month, utc_time.day,
            utc_time.hour, utc_time.minute, utc_time.second,
            location.latitude, location.longitude);
    } else {
        printf("  GPS:  Searching / No Fix\n");
    }
    printf("\n");

    _last_print_ms = now_ms;

    // Accumulate data
    if (_reading_counter > 1) _readings += ",";
    _readings += buildJsonReading();

    // Transmit batch
    if (_reading_counter >= SEND_BATCH_SIZE) {
        if (!_nm.IsBusy()) {
            std::string payload = "[" + _readings + "]";
            if (_nm.StartSend(payload.c_str())) {
                _readings = "";
                _reading_counter = 0;
            }
        } else {
            printf("NetworkManager is busy, buffering readings...\n\n");
            // Wait for next loop if still busy, keep counter at SEND_BATCH_SIZE
            _reading_counter--;
        }

        if (_reading_counter > 25) {
            printf("Network offline too long! Dropping oldest readings to prevent memory overflow.\n");
            _readings = "";
            _reading_counter = 0;
        }
    }
}

void GrassMonitor::initSystemLeds() {
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);

    gpio_init(LED_YELLOW);
    gpio_set_dir(LED_YELLOW, GPIO_OUT);

    // Start with LEDs off
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_YELLOW, 0);
}

void GrassMonitor::updateSystemLeds(SystemState state) {
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    static SystemState last_state    = SystemState::READING;
    static uint32_t state_start_ms   = 0;

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

void GrassMonitor::scanI2c(i2c_inst_t* i2c, const char* label) {
    printf("Scanning %s...\n", label);
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy;
        if (i2c_read_blocking(i2c, addr, &dummy, 1, false) >= 0) {
            printf("  Device found at 0x%02X\n", addr);
        }
    }
    printf("\n");
}

void GrassMonitor::calibrateSensors() {
    printf("\nCalibrating sensors based on ground height (keep the mower still)...\n");

    int cal_samples = 0;
    while(cal_samples < 25) {
        updateSystemLeds(SystemState::CALIBRATING);

        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (_tof_ok && _tof.dataReady()) _processor.parseTof(_tof.readDistance());
        if (_sonic_ok && (now_ms % 50 == 0)) _processor.parseSonic(_ultrasonic.readDistance());

        sleep_ms(20);
        cal_samples++;
    }
    _processor.calibrate();
    printf("Calibration complete.\n\n");
}

std::string GrassMonitor::buildJsonReading() {
    std::string obj = "{";
    RawData raw = _processor.raw();
    CalibrationData cal = _processor.get_calibration();

    // --- Processed data ---
    if (_tof_ok && cal.tof_calibrated) {
        obj += "\"grassHeightTof\":" + std::to_string(_processor.grassHeightTof());
    } else {
        obj += "\"grassHeightTof\":null";
    }

    if (_sonic_ok && cal.sonic_calibrated) {
        obj += ",\"grassHeightSonicMedian\":" + std::to_string(_processor.grassHeightSonicMedian());
        obj += ",\"grassHeightSonicAccel\":"  + std::to_string(_processor.grassHeightSonicAccel());
    } else {
        obj += ",\"grassHeightSonicMedian\":null";
        obj += ",\"grassHeightSonicAccel\":null";
    }

    // --- Raw data ---
    obj += ",\"sonic_raw_mm\":"  + (_sonic_ok ? std::to_string(raw.sonic_mm) : "null");
    obj += ",\"tof_raw_mm\":"    + (_tof_ok ? std::to_string(raw.tof_mm) : "null");
    obj += ",\"accel_raw_x\":"   + (_accel_ok ? std::to_string(raw.accel_x) : "null");
    obj += ",\"accel_raw_y\":"   + (_accel_ok ? std::to_string(raw.accel_y) : "null");
    obj += ",\"accel_raw_z\":"   + (_accel_ok ? std::to_string(raw.accel_z) : "null");
    obj += ",\"temperature\":"   + std::to_string(raw.temperature_c);

    // --- GPS data ---
    if (_gps_ok && utc_time.valid) {
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