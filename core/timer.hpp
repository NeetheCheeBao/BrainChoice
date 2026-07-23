#pragma once
#include <windows.h>

class Timer {
public:
    Timer() {
        QueryPerformanceFrequency(&freq_);
        QueryPerformanceCounter(&start_);
        last_ = start_;
    }

    float elapsed() const {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return seconds(start_, now);
    }

    float tick() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        const float dt = seconds(last_, now);
        last_ = now;
        return dt;
    }

private:
    float seconds(const LARGE_INTEGER& a, const LARGE_INTEGER& b) const {
        return (float)(b.QuadPart - a.QuadPart) / (float)freq_.QuadPart;
    }

    LARGE_INTEGER freq_{};
    LARGE_INTEGER start_{};
    LARGE_INTEGER last_{};
};
