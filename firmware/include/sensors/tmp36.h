#pragma once
#include "pico/stdlib.h"

class TMP36 {
public:
    explicit TMP36(uint adc_pin);

    bool init();
    float readTemperature();
    bool isConnected();

private:
    uint _adc_pin;
    uint _adc_channel;
};