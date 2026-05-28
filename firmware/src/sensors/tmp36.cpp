#include "sensors/tmp36.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"
#include "config.h"

TMP36::TMP36(uint adc_pin) : _adc_pin(adc_pin) {
    _adc_channel = _adc_pin - 26;
}

bool TMP36::init() {
    if (_adc_pin < 26 || _adc_pin > 28) return false;
    adc_gpio_init(_adc_pin);
    return true;
}

float TMP36::readTemperature() {
    adc_select_input(_adc_channel);

    // oversample to kill adc noise
    uint32_t raw_sum = 0;
    const int num_samples = 50;

    for (int i = 0; i < num_samples; i++) {
        raw_sum += adc_read();
        sleep_us(100);
    }

    float raw_avg = (float)raw_sum / num_samples;

    // calculate raw voltage
    float voltage = raw_avg * (3.3f / 4095.0f);

    // calculate temp and apply the hardware calibration offset
    float raw_temp = (voltage - 0.5f) * 100.0f;
    return raw_temp + Config::Sensor::TMP36_OFFSET_C;
}

bool TMP36::isConnected() {
    // temporarily pull up and down to detect floating state
    gpio_pull_up(_adc_pin);
    sleep_ms(2);
    float temp_up = readTemperature();

    gpio_pull_down(_adc_pin);
    sleep_ms(2);
    float temp_down = readTemperature();

    gpio_disable_pulls(_adc_pin);
    sleep_ms(2);

    float diff = temp_up - temp_down;
    if (diff < 0) diff = -diff;

    // if floating diff is huge, if connected diff is minimal
    if (diff > 10.0f) {
        return false;
    }

    float temp = readTemperature();
    if (temp < -40.0f || temp > 125.0f) {
        return false;
    }
    return true;
}