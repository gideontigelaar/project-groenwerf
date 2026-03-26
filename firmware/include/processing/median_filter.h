#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>

class MedianFilter {
public:
    explicit MedianFilter(size_t window = 5)
        : _window(window), _buf(window), _head(0), _size(0), _count(0) {}

    void push(float value);
    float get() const;

    size_t count() const { return _count; }
    void reset() {
        _head = 0;
        _size = 0;
        _count = 0;
    }

private:
    std::vector<float> _buf;
    size_t _window;
    size_t _head;
    size_t _size;
    size_t _count;
};