#include "processing/median_filter.h"

void MedianFilter::push(float value) {
    _buf[_head] = value;
    _head = (_head + 1) % _window;
    if(_size < _window) _size++;
    _count++;
}

float MedianFilter::get() const {
    if(_size == 0) return 0.0f;

    std::vector<float> sorted(_buf.begin(), _buf.begin() + _size);
    std::sort(sorted.begin(), sorted.end());

    // mathematical median for even-sized arrays
    if (_size % 2 == 0) {
        return (sorted[(_size / 2) - 1] + sorted[_size / 2]) / 2.0f;
    } else {
        return sorted[_size / 2];
    }
}

float MedianFilter::variance() const {
    if (_size < 2) return 0.0f;

    float mean = 0.0f;
    for (size_t i = 0; i < _size; i++) {
        mean += _buf[i];
    }
    mean /= _size;

    float var = 0.0f;
    for (size_t i = 0; i < _size; i++) {
        var += (_buf[i] - mean) * (_buf[i] - mean);
    }
    return var / _size;
}