//integrated: message-path adapters

/*
Hides: how Inodh's message router reaches Andrew's modules, and where
       Inodh's dashboard messages go
  HeartbeatLink  Inodh's HeartbeatMonitor -> Andrew's AdcHeartbeats::heartbeatReceived
                 (text time ignored: ADC heartbeats reads the simulated clock)
  InventoryLink  Inodh's InventoryStore   -> Andrew's InventoryRecord
                 setCount(new) becomes deposit or withdraw of the difference, so
                 Andrew's rules, history entries and low-stock alarms apply
  Inbox          Inodh's DashboardOutput  -> a per-user message list the page shows
*/
#pragma once

#include "gui/integrated/convert.hpp"

#include <map>
#include <string>
#include <vector>

#include "behaviour_hiding/adc_heartbeats.hpp"
#include "behaviour_hiding/inventory_record.hpp"
#include "gui/integrated/feed.hpp"
#include "tests/fakes/fake_heartbeat_counter.hpp"

class HeartbeatLink : public HeartbeatMonitor {
public:
    HeartbeatLink(AdcHeartbeats& heartbeats, FakeHeartbeatCounter& counts,
                  std::map<hms::behaviour::AdcId, long long>& lastHeard, const hms::behaviour::Clock& clock)
        : heartbeats(heartbeats), counts(counts), lastHeard(lastHeard), clock(clock) {}

    void heartbeat(int adcId, const std::string&) override {
        hms::behaviour::AdcId adc = convert::fromSwAdc(adcId);
        if (heartbeats.heartbeatReceived(adc) != HeartbeatVerdict::Accepted) return;
        counts.record(adc, clock.now());
        lastHeard[adc] = clock.now();
    }

private:
    AdcHeartbeats& heartbeats;
    FakeHeartbeatCounter& counts;
    std::map<hms::behaviour::AdcId, long long>& lastHeard;
    const hms::behaviour::Clock& clock;
};

class InventoryLink : public InventoryStore {
public:
    InventoryLink(InventoryRecord& record, const hms::behaviour::Clock& clock, Feed& feed)
        : record(record), clock(clock), feed(feed) {}

    bool hasMedication(int adcId, const std::string& medication) const override {
        return record.count(convert::fromSwAdc(adcId), medication).has_value();
    }
    int count(int adcId, const std::string& medication) const override {
        return record.count(convert::fromSwAdc(adcId), medication).value_or(0);
    }
    void setCount(int adcId, const std::string& medication, int newCount) override;

private:
    InventoryRecord& record;
    const hms::behaviour::Clock& clock;
    Feed& feed;
};

class Inbox : public DashboardOutput {
public:
    struct Message { long long time; std::string text; };
    static constexpr std::size_t LIMIT = 50;   // per user; oldest dropped

    explicit Inbox(const hms::behaviour::Clock& clock) : clock(clock) {}

    void deliver(const std::string& userId, const std::string& message) override {
        std::vector<Message>& box = boxes[userId];
        box.push_back({clock.now(), message});
        if (box.size() > LIMIT) box.erase(box.begin());
    }

    std::vector<Message> for_(const std::string& userId) const {
        auto found = boxes.find(userId);
        return found == boxes.end() ? std::vector<Message>{} : found->second;
    }

private:
    const hms::behaviour::Clock& clock;
    std::map<std::string, std::vector<Message>> boxes;
};
