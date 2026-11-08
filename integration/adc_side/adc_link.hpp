//ADC link

/*
Hides: when the ADC's messages are sent to the HMS: a background thread, so
       nothing the ADC does ever waits on the network; a queue of up to 200
       messages while the HMS is unreachable (oldest dropped first, and
       counted); heartbeats every period while the link is up (never queued);
       retrying every 2 s while it is down

Interface
takes: plain-text messages from the console observer and the transceiver
sends: them to the HMS through HmsClient, in order; one [HMS-LINK] line on
       stderr each time the link goes up or down
uses: HmsClient
*/
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "hms_client.hpp"

class AdcLink {
public:
    static constexpr std::size_t QUEUE_LIMIT = 200;
    static constexpr int RETRY_MS = 2000;

    AdcLink(int adcId, std::unique_ptr<HmsClient> client, int heartbeatSeconds);
    ~AdcLink();   // sends what it can for up to ~3 s, then stops the thread

    void enqueue(const std::string& message);
    // A ping the caller waits for (the ADC's "Ping HMS Network" option).
    bool pingNow();

    bool up() const { return linkUp; }
    std::size_t waiting() const;
    std::size_t dropped() const { return droppedCount; }
    int adcId() const { return id; }
    // The demo "Swap ADC": heartbeats from now on are for this ADC. Queued
    // messages keep the id they were made with.
    void setAdcId(int adc) { id = adc; }

private:
    void run();
    bool trySend(const std::string& message);
    void setUp(bool nowUp);

    std::atomic<int> id;
    std::unique_ptr<HmsClient> client;
    const int heartbeatMs;

    mutable std::mutex queueLock;
    std::condition_variable wake;
    std::deque<std::string> queue;
    std::atomic<bool> linkUp{false};
    std::atomic<bool> stopping{false};
    std::atomic<std::size_t> droppedCount{0};
    std::atomic<bool> everReported{false};
    std::thread sender;
};
