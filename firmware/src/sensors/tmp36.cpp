#include "sensors/tmp36.h"
#include "hardware/adc.h"

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
    uint16_t raw = adc_read();

    float voltage = raw * (3.3f / 4095.0f);

    return (voltage - 0.5f) * 100.0f;
}

bool TMP36::isConnected() {
    float temp = readTemperature();
    if (temp < -40.0f || temp > 125.0f) {
        return false;
    }
    return true;
}