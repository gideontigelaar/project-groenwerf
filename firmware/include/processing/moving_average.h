#pragma once
#include <cstdint>
#include <vector>

class MovingAverage {
public:
    void push(float value);
    float get();

    size_t count() { return _count; }
    void reset() { _count = 0; _head = 0; }

private:
    std::vector<float> _buf{};
    size_t _head = 0;
    size_t _count = 10;
};