#include "grass_monitor.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "logger.h"

GrassMonitor::GrassMonitor()
    : _tof(i2c0, TofSensor::DEFAULT_ADDR),
      _accel(i2c1, ADXL345::DEFAULT_ADDR),
      _ultrasonic(Config::Pins::RCWL_TRIG, Config::Pins::RCWL_ECHO),
      _tmp36(Config::Pins::TMP36_PIN)
{
}

void GrassMonitor::init() {
    stdio_init_all();

    // init leds
    initSystemLeds();

    // init adc and internal temp sensor
    adc_init();
    adc_set_temp_sensor_enabled(true);

    sleep_ms(100);
    LOG_INFO("=== Grass Monitor Pico ===");

    // wi-fi connection
    LOG_INFO("Init: Connecting to Wi-Fi...");
    _nm.ConnectInitial();

    // init i2c0 for tof
    LOG_INFO("Init: Configuring I2C buses...");
    i2c_init(i2c0, Config::I2C_FREQ);
    gpio_set_function(Config::Pins::TOF_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(Config::Pins::TOF_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(Config::Pins::TOF_I2C_SDA);
    gpio_pull_up(Config::Pins::TOF_I2C_SCL);

    // init i2c1 for accelerometer
    i2c_init(i2c1, Config::I2C_FREQ);
    gpio_set_function(Config::Pins::ACCEL_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(Config::Pins::ACCEL_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(Config::Pins::ACCEL_I2C_SDA);
    gpio_pull_up(Config::Pins::ACCEL_I2C_SCL);

    scanI2c(i2c0, "I2C0 (ToF)");
    scanI2c(i2c1, "I2C1 (Accel)");

    // initialize sensors
    LOG_INFO("Init: Initializing sensors...");
    _tof_ok   = _tof.init();
    _accel_ok = _accel.init();
    _sonic_ok = _ultrasonic.init();
    _tmp36_ok = _tmp36.init();

    LOG_INFO("  - ToF Sensor   : %s", _tof_ok ? "OK" : "FAIL");
    LOG_INFO("  - Sonic (RCWL) : %s", _sonic_ok ? "OK" : "FAIL");
    LOG_INFO("  - Accelerometer: %s", _accel_ok ? "OK" : "FAIL");
    LOG_INFO("  - TMP36 Temp   : %s", (_tmp36_ok && _tmp36.isConnected()) ? "OK" : "FAIL (using internal fallback)");

    if (!_tmp36_ok || !_tmp36.isConnected()) {
        _tmp36_ok = false;
    }

    if (!_tof_ok && !_sonic_ok) {
        LOG_ERROR("FATAL: Neither ToF nor Sonic sensor found. Halting.");
        while (true) {
            updateSystemLeds(SystemState::ERROR_WARNING);
            sleep_ms(100);
        }
    }

    if (_tof_ok) _tof.startContinuous(Config::Timing::TOF_INTERVAL_MS);

    // calibration phase
    LOG_INFO("Init: Starting sensor calibration...");
    calibrateSensors();

    // init gps in background
    LOG_INFO("Init: Initializing GPS...");
    _gps_ok = gps_init();
    LOG_INFO("  - GPS Module   : %s", _gps_ok ? "OK (Waiting for fix)" : "FAIL");

    LOG_INFO("Init: System ready. Starting monitor loop...\n");
}

void GrassMonitor::run() {
    while (true) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        pollNetwork();

        if (_nm.IsHalted()) {
            sleep_ms(100);
            continue;
        }

        pollSensors(now_ms);
        processAndSendBatch(now_ms);

        uint32_t loop_elapsed = to_ms_since_boot(get_absolute_time()) - now_ms;
        if (loop_elapsed < Config::Timing::LOOP_TICK_MS) {
            sleep_ms(Config::Timing::LOOP_TICK_MS - loop_elapsed);
        }
    }
}

void GrassMonitor::pollNetwork() {
    _nm.Poll();
    SystemState current_state = SystemState::READING;
    int wifi_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (_nm.IsHalted()) current_state = SystemState::HALTED;
    else if (wifi_status != CYW43_LINK_UP) current_state = SystemState::WIFI_RECONNECTING;
    else if (_nm.HasError()) current_state = SystemState::ERROR_WARNING;
    else if (_nm.IsBusy()) current_state = SystemState::TRANSMITTING;

    updateSystemLeds(current_state);
}

void GrassMonitor::pollSensors(uint32_t now_ms) {
    if (_tof_ok && (now_ms - _last_tof_ms) >= Config::Timing::TOF_INTERVAL_MS) {
        if (_tof.dataReady()) {
            _processor.parseTof(_tof.readDistance());
            _last_tof_ms = now_ms;
        }
    }

    if ((now_ms - _last_sonic_ms) >= Config::Timing::SONIC_INTERVAL_MS) {
        updateTemperature();
        if (_sonic_ok) {
            float current_temp_c = _processor.raw().temperature_c;
            _processor.parseSonic(_ultrasonic.readDistance(current_temp_c));
        }
        _last_sonic_ms = now_ms;
    }

    if (_accel_ok && (now_ms - _last_accel_ms) >= Config::Timing::ACCEL_INTERVAL_MS) {
        AccelData accel_data = _accel.read();
        _processor.parseAccel(accel_data.x, accel_data.y, accel_data.z, now_ms);
        _last_accel_ms = now_ms;
    }

    if (_gps_ok) {
        if (gps_get_ubx_packet()) gps_parse_ubx_data();
    }
}

void GrassMonitor::updateTemperature() {
    float current_temp_c = 20.0f;
    _using_tmp36 = false;

    if (_tmp36_ok) {
        float t = _tmp36.readTemperature();
        // if temp sensor disconnects, lock out to prevent bad readings
        if (t >= -10.0f && t <= 60.0f) {
            current_temp_c = t;
            _using_tmp36 = true;
        } else {
            _tmp36_ok = false;
        }
    }

    if (!_using_tmp36) {
        // fallback to internal temp sensor
        adc_select_input(Config::Pins::INTERNAL_TEMP_ADC);
        const float conversion_factor = 3.3f / (1 << 12);
        float adc_voltage = (float)adc_read() * conversion_factor;
        // subtract 5.0f for chip heat offset
        current_temp_c = 27.0f - (adc_voltage - 0.706f) / 0.001721f - 5.0f;
    }

    _processor.setTemperature(current_temp_c);
}

void GrassMonitor::processAndSendBatch(uint32_t now_ms) {
    if ((now_ms - _last_print_ms) < Config::Timing::PRINT_INTERVAL_MS) return;

    _reading_counter++;
    RawData raw = _processor.raw();

    // calculate temp correction (speed of sound diff)
    float base_speed = 331.3f;
    float current_speed = 331.3f + (0.606f * raw.temperature_c);
    float temp_correction_perc = ((current_speed / base_speed) - 1.0f) * 100.0f;

    LOG_RAW("--- Reading (%d/%d) ---\n", _reading_counter, Config::Network::SEND_BATCH_SIZE);
    LOG_RAW("   Raw:  ToF:        %4d mm |  Sonic:      %4d mm |  Accel:       x(%5.2f) y(%5.2f) z(%5.2f)\n",
        raw.tof_mm, raw.sonic_mm, raw.accel_x, raw.accel_y, raw.accel_z);

    LOG_RAW("   Proc: ToF:        %4d mm |  Sonic:      %4d mm |  Temp:        %4.1f C (%s)\n",
        _processor.grassHeightTof(), _processor.grassHeightSonicMedianAccel(),
        raw.temperature_c, _using_tmp36 ? "TMP36" : "Internal");

    LOG_RAW("   SDbg: SonicMed:   %4d mm |  SonicAccel: %4d mm |  TempShft:    %+.1f%%\n",
        _processor.grassHeightSonicMedian(), _processor.grassHeightSonicAccel(),
        temp_correction_perc);

    if (_gps_ok && utc_time.valid) {
        LOG_RAW("   GPS:  %04d-%02d-%02d %02d:%02d:%02d |  Lat: %.6f  Lon: %.6f\n",
            utc_time.year, utc_time.month, utc_time.day,
            utc_time.hour, utc_time.minute, utc_time.second,
            location.latitude, location.longitude);
    } else {
        LOG_RAW("   GPS:  Searching / No Fix\n");
    }
    LOG_RAW("\n");

    _last_print_ms = now_ms;

    // accumulate data
    if (_reading_counter > 1) _readings += ",";
    _readings += buildJsonReading();

    // transmit batch
    if (_reading_counter >= Config::Network::SEND_BATCH_SIZE) {
        if (!_nm.IsBusy()) {
            std::string payload = "[" + _readings + "]";
            if (_nm.StartSend(payload.c_str())) {
                _readings = "";
                _reading_counter = 0;
            }
        } else {
            LOG_WARN("Network: Busy, buffering readings...");
            _reading_counter--;
        }

        // Drop batch if too large
        if (_reading_counter > 25) {
            LOG_ERROR("Network: Offline too long! Dropping oldest readings.");
            _readings = "";
            _reading_counter = 0;
        }
    }
}

void GrassMonitor::initSystemLeds() {
    gpio_init(Config::Pins::LED_GREEN);
    gpio_set_dir(Config::Pins::LED_GREEN, GPIO_OUT);

    gpio_init(Config::Pins::LED_YELLOW);
    gpio_set_dir(Config::Pins::LED_YELLOW, GPIO_OUT);

    gpio_put(Config::Pins::LED_GREEN, 0);
    gpio_put(Config::Pins::LED_YELLOW, 0);
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
            gpio_put(Config::Pins::LED_GREEN, 1);
            gpio_put(Config::Pins::LED_YELLOW, 1);
            break;
        case SystemState::READING:
            gpio_put(Config::Pins::LED_GREEN, 1);
            gpio_put(Config::Pins::LED_YELLOW, 0);
            break;
        case SystemState::TRANSMITTING:
            gpio_put(Config::Pins::LED_GREEN, led_blink_start_off ? 1 : 0);
            gpio_put(Config::Pins::LED_YELLOW, 0);
            break;
        case SystemState::ERROR_WARNING:
            gpio_put(Config::Pins::LED_GREEN, 0);
            gpio_put(Config::Pins::LED_YELLOW, 1);
            break;
        case SystemState::WIFI_RECONNECTING:
            gpio_put(Config::Pins::LED_GREEN, 0);
            gpio_put(Config::Pins::LED_YELLOW, led_blink_start_off ? 1 : 0);
            break;
    }
}

void GrassMonitor::scanI2c(i2c_inst_t* i2c, const char* label) {
    LOG_INFO("  Scanning %s...", label);
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy;
        if (i2c_read_blocking(i2c, addr, &dummy, 1, false) >= 0) {
            LOG_INFO("    - Device found at 0x%02X", addr);
        }
    }
}

void GrassMonitor::calibrateSensors() {
    LOG_INFO("  Calibrating sensors from ground height...");

    uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    uint32_t last_tof_ms = 0;
    uint32_t last_sonic_ms = 0;
    int tof_samples = 0;
    int sonic_samples = 0;

    auto done = [&]() {
        bool tof_ready = !_tof_ok || _processor.get_calibration().tof_calibrated;
        bool sonic_ready = !_sonic_ok || _processor.get_calibration().sonic_calibrated;
        return tof_ready && sonic_ready;
    };

    while (!done() && (to_ms_since_boot(get_absolute_time()) - start_ms < 5000)) {
        updateSystemLeds(SystemState::CALIBRATING);
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        if (_tof_ok && (now_ms - last_tof_ms >= Config::Timing::TOF_INTERVAL_MS)) {
            uint16_t d = _tof.readDistance();
            if (d > 0) {
                _processor.parseTof(d);
                tof_samples++;
            }
            last_tof_ms = now_ms;
        }

        if (_sonic_ok && (now_ms - last_sonic_ms >= Config::Timing::SONIC_INTERVAL_MS)) {
            uint16_t d = _ultrasonic.readDistance();
            if (d > 0) {
                _processor.parseSonic(d);
                sonic_samples++;
            }
            last_sonic_ms = now_ms;
        }

        sleep_ms(10);
    }

    CalibrationData cal = _processor.get_calibration();
    if (done()) {
        LOG_INFO("  Calibration complete (ToF: %d samples, Sonic: %d samples)", tof_samples, sonic_samples);
        LOG_INFO("  Offsets applied -> ToF: %+.1f mm | Sonic: %+.1f mm",
            cal.tof_offset_mm, cal.sonic_offset_mm);
    } else {
        LOG_WARN("  Calibration timed out! ToF: %s, Sonic: %s",
            (_tof_ok && !cal.tof_calibrated) ? "FAIL" : "OK",
            (_sonic_ok && !cal.sonic_calibrated) ? "FAIL" : "OK");
        LOG_WARN("  Partial offsets -> ToF: %+.1f mm | Sonic: %+.1f mm",
            cal.tof_offset_mm, cal.sonic_offset_mm);
    }
}

std::string GrassMonitor::buildJsonReading() {
    std::string obj = "{";
    RawData raw = _processor.raw();
    CalibrationData cal = _processor.get_calibration();

    if (_tof_ok && cal.tof_calibrated) {
        obj += "\"ght\":" + std::to_string(_processor.grassHeightTof());
    } else {
        obj += "\"ght\":null";
    }

    if (_sonic_ok && cal.sonic_calibrated) {
        obj += ",\"ghs\":" + std::to_string(_processor.grassHeightSonicMedianAccel());
    } else {
        obj += ",\"ghs\":null";
    }

    obj += ",\"sr\":"  + (_sonic_ok ? std::to_string(raw.sonic_mm) : "null");
    obj += ",\"tr\":"    + (_tof_ok ? std::to_string(raw.tof_mm) : "null");
    obj += ",\"ax\":"   + (_accel_ok ? std::to_string(raw.accel_x) : "null");
    obj += ",\"ay\":"   + (_accel_ok ? std::to_string(raw.accel_y) : "null");
    obj += ",\"az\":"   + (_accel_ok ? std::to_string(raw.accel_z) : "null");
    obj += ",\"t\":"   + std::to_string(raw.temperature_c);

    if (_gps_ok && utc_time.valid) {
        char ts[32];
        snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02dZ",
            utc_time.year, utc_time.month, utc_time.day,
            utc_time.hour, utc_time.minute, utc_time.second);
        char lat_str[32], lon_str[32];
        snprintf(lat_str, sizeof(lat_str), "%.7f", location.latitude);
        snprintf(lon_str, sizeof(lon_str), "%.7f", location.longitude);

        obj += ",\"m_at\":\"" + std::string(ts) + "\"";
        obj += ",\"lt\":" + std::string(lat_str);
        obj += ",\"ln\":" + std::string(lon_str);
    } else {
        obj += ",\"m_at\":null,\"lt\":null,\"ln\":null";
    }

    obj += "}";
    return obj;
}