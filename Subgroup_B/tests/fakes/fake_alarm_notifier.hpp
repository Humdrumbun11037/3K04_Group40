//fake alarm notifier

/*
Records every alarm raised instead of notifying anyone, so a test can check
exactly which alarms a module raised and how many.
*/
#pragma once

#include <string>
#include <vector>

#include "behaviour_hiding/interfaces/alarm_notifier.hpp"

class FakeAlarmNotifier : public hms::behaviour::AlarmNotifier {
public:
    struct RaisedAlarm {
        hms::behaviour::AdcId adc;
        AlarmType type;
        std::string detail;
    };

    void raise(const hms::behaviour::AdcId& adc, AlarmType type,
               const std::string& detail = "") override {
        raised.push_back({adc, type, detail});
    }

    std::vector<RaisedAlarm> raised;
};
