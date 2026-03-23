#pragma once
#include <cstdint>
#include <vector>

class MovingAverage {
public:
    explicit MovingAverage(size_t window = 10) : _window(window) {
        _buf.resize(window, 0.0f);
    }

    void push(float value);
    float get() const;


    size_t count() const { return _count; }

    void reset() {
        _count = 0;
        _head = 0;
        std::fill(_buf.begin(), _buf.end(), 0.0f);
    }

private:
    std::vector<float> _buf{};
    size_t _window;
    size_t _head = 0;
    size_t _count = 0;
};