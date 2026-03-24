#include "processing/median_filter.h"

void MedianFilter::push(float value) {
    if (_buf.size() >= _window) {
        _buf.erase(_buf.begin()); // circular would be better long term, fine for now
    }
    _buf.push_back(value);
    _count++;
}

float MedianFilter::get() const {
    if (_buf.empty()) return 0.0f;

    std::vector<float> sorted(_buf);           // copy — don't mutate the buffer
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());

    return sorted[sorted.size() / 2];
}