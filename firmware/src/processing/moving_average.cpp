#include "processing/moving_average.h"

// Add new value to the buffer, evicting oldest if full
void MovingAverage::push(float value) {
    _buf[_head % _window] = value;
    _head = (_head + 1) % _window;
    if (_count < _window) ++_count;
}

// Calculate and return the average of the values in the buffer
float MovingAverage::get() const {
    if (_count == 0) return 0.0f;
    float sum = 0.0f;
    for (size_t i = 0; i < _count; ++i)
        sum += _buf[i];
    return sum / static_cast<float>(_count);
}