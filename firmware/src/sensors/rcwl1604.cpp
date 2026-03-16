#include "sensors/rcwl1604.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <cstdio>

static constexpr uint32_t ECHO_TIMEOUT_US = 30000;

RCWL1604::RCWL1604(uint trig_pin, uint echo_pin)
    : _trig(trig_pin), _echo(echo_pin) {}

bool RCWL1604::init() {
    gpio_init(_trig);
    gpio_set_dir(_trig, GPIO_OUT);
    gpio_put(_trig, 0);

    gpio_init(_echo);
    gpio_set_dir(_echo, GPIO_IN);

    sleep_ms(100);
    return true;
}

uint16_t RCWL1604::readDistance() {
    // pulse trig high for 10us to start a measurement
    gpio_put(_trig, 1);
    sleep_us(10);
    gpio_put(_trig, 0);

    // wait for echo to go high, timeout after ECHO_TIMEOUT_US
    uint32_t t0 = time_us_32();
    while(!gpio_get(_echo)) {
        if((time_us_32() - t0) > ECHO_TIMEOUT_US) return 0;
    }

    // measure how long echo stays high
    uint32_t start = time_us_32();
    while(gpio_get(_echo)) {
        if((time_us_32() - start) > ECHO_TIMEOUT_US) return 0;
    }
    uint32_t duration_us = time_us_32() - start;

    // duration_us / 5.8 converts to mm
    return (uint16_t)(duration_us / 5.8f);
}