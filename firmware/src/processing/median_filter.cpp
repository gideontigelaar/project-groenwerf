#include "processing/median_filter.h"

void MedianFilter::push(float value) {
    _buf[_head] = value;
    _head = (_head + 1) % _window;
    if(_size < _window) _size++;
}

float MedianFilter::get() const {
    if(_size == 0) return 0.0f;

    for (size_t i = 0; i < _size; ++i) {
        _sort_buf[i] = _buf[i];
    }

    std::nth_element(_sort_buf.begin(), _sort_buf.begin() + _size / 2, _sort_buf.begin() + _size);

    return _sort_buf[_size / 2];
}