//fake clock

/*
A Clock the test (or demo) controls: set it to any time, or move it forward,
e.g. advance(180) to make an ADC miss three heartbeat periods instantly.
*/
#pragma once

#include "behaviour_hiding/interfaces/clock.hpp"

class FakeClock : public hms::behaviour::Clock {
public:
    explicit FakeClock(long long start = 0) : current(start) {}

    long long now() const override { return current; }

    void set(long long time) { current = time; }
    void advance(long long seconds) { current += seconds; }

private:
    long long current;
};
