#include "processing/median_filter.h"

void MedianFilter::push(float value) {
    _buf[_head] = value;
    _head = (_head + 1) % _window;
    if (_size < _window) _size++;
    _count++;
}

float MedianFilter::get() const {
    if (_size == 0) return 0.0f;

    std::vector<float> sorted(_buf.begin(), _buf.begin() + _size);
    std::nth_element(sorted.begin(), sorted.begin() + _size / 2, sorted.end());

    return sorted[_size / 2];
}