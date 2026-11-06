//integrated: small adapters between Inodh's interfaces and Andrew's

/*
Hides: how each side's interface is met by the other side's code
  TextClock        Inodh's Clock (text)        <- the one simulated clock (seconds)
  RegistryAdapter  Andrew's AdcRegistry        <- Inodh's ADClist
  EventToHistory   Inodh's EventRecorder       -> Andrew's History (one history for everyone)
*/
#pragma once

#include "gui/integrated/convert.hpp"

#include <string>
#include <vector>

#include "behaviour_hiding/interfaces/adc_registry.hpp"
#include "behaviour_hiding/interfaces/clock.hpp"
#include "behaviour_hiding/interfaces/history.hpp"

// Inodh's code reads the same simulated time as Andrew's, as text.
class TextClock : public Clock {
public:
    explicit TextClock(const hms::behaviour::Clock& seconds) : seconds(seconds) {}
    std::string now() const override { return convert::toText(seconds.now()); }

private:
    const hms::behaviour::Clock& seconds;
};

// Andrew's modules ask Inodh's registry which ADCs exist. Every registered ADC
// is active: Inodh's registry removes an ADC rather than marking it decommissioned.
class RegistryAdapter : public hms::behaviour::AdcRegistry {
public:
    explicit RegistryAdapter(const ADClist& list) : list(list) {}

    bool isActive(const hms::behaviour::AdcId& adc) const override { return adc > 0 && list.exists(adc); }

    std::vector<hms::behaviour::AdcId> activeAdcs() const override {
        std::vector<hms::behaviour::AdcId> ids;
        for (const ADC& adc : list.all()) ids.push_back(adc.id);
        return ids;
    }

private:
    const ADClist& list;
};

// Inodh's modules log events here; they become entries in Andrew's history, so
// the daily report, the summaries and history search see both sides' events.
// Inodh's record() returns nothing, so a failed write can only be counted.
class EventToHistory : public EventRecorder {
public:
    EventToHistory(hms::behaviour::History& history, const hms::behaviour::Clock& clock)
        : history(history), clock(clock) {}

    void record(const Event& event) override;

    int failures = 0;   // writes the history refused

private:
    hms::behaviour::History& history;
    const hms::behaviour::Clock& clock;
};
