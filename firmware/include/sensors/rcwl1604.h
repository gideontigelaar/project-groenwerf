#pragma once
#include "pico/stdlib.h"
#include <cstdint>

class RCWL1604 {
public:
    explicit RCWL1604(uint trig_pin, uint echo_pin);

    bool init();
    uint16_t readDistance(); // returns distance in mm

private:
    uint _trig;
    uint _echo;
};