#include "processing/moving_average.h"

// Add new value to the buffer, evicting oldest if full
void MovingAverage::push(float value) {
    if (_buf.size() >= _count) {
        _buf.erase(_buf.begin());
    }
    _buf.push_back(value);
}

// Calculate and return the average of the values in the buffer
float MovingAverage::get() {
    if (_buf.empty()) return 0.0f;
    float sum = 0;
    for (size_t i = 0; i < _buf.size(); ++i)
        sum += _buf[i];
    return sum / _buf.size();
}