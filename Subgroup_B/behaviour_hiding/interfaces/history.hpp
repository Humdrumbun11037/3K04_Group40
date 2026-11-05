//history (interface only)

/*
Owned by history storage and audit trail (software decisions). This file only
states what the behaviour modules need from it: a way to store one entry.

Interface
takes: one history entry (kind, who, detail, time)
sends: whether it was stored
*/
#pragma once

#include <string>

#include "behaviour_hiding/interfaces/adc_id.hpp"

namespace hms::behaviour {

struct HistoryEntry {
    std::string kind;        // e.g. "AlarmResolved"
    std::string who;         // user name, or "HMS" for automatic actions
    std::string detail;      // e.g. "alarm 3: replaced network cable"
    long long time;          // seconds, from the Clock
    AdcId adc;               // the ADC it concerns; 0 means none
    std::string medication;  // the medication it concerns; empty means none
};

enum class HistoryResult { Stored, StorageFailure };

class History {
public:
    virtual ~History() = default;

    // { Pre: none }
    //   record(entry)
    // { Post: Stored         => entry is kept permanently
    //         StorageFailure => nothing was kept }
    virtual HistoryResult record(const HistoryEntry& entry) = 0;
};

}
