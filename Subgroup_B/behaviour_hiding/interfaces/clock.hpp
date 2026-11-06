//clock (interface only)

/*
Owned by hardware hiding, not behaviour hiding. This file only states what the
behaviour modules need from a clock; the real clock is written by its owner.
Behaviour modules get a Clock& through their constructor, so tests can pass a
FakeClock instead.

Team assumption: all times are hospital local time. There is no time-zone
handling anywhere in the HMS.

Interface
sends: the current time
*/
#pragma once

namespace hms::behaviour {

class Clock {
public:
    virtual ~Clock() = default;

    // { Pre: none }
    //   now()
    // { Post: returns the current time in whole seconds since
    //         1970-01-01 00:00 hospital local time }
    virtual long long now() const = 0;
};

}
