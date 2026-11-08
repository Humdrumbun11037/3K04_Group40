//ADC link

#include "adc_link.hpp"

#include <chrono>
#include <iostream>

using Steady = std::chrono::steady_clock;

AdcLink::AdcLink(int adcId, std::unique_ptr<HmsClient> client, int heartbeatSeconds)
    : id(adcId), client(std::move(client)), heartbeatMs(heartbeatSeconds * 1000) {
    sender = std::thread([this] { run(); });
}

AdcLink::~AdcLink() {
    stopping = true;
    wake.notify_all();
    if (sender.joinable()) sender.join();
}

void AdcLink::enqueue(const std::string& message) {
    {
        std::lock_guard<std::mutex> guard(queueLock);
        if (queue.size() >= QUEUE_LIMIT) {
            queue.pop_front();
            droppedCount++;
        }
        queue.push_back(message);
    }
    wake.notify_all();
}

std::size_t AdcLink::waiting() const {
    std::lock_guard<std::mutex> guard(queueLock);
    return queue.size();
}

bool AdcLink::pingNow() {
    bool ok = client->ping();
    setUp(ok);
    return ok;
}

bool AdcLink::trySend(const std::string& message) {
    bool ok = client->send(message) == HmsClient::Result::Acked;
    setUp(ok);
    return ok;
}

void AdcLink::setUp(bool nowUp) {
    bool was = linkUp.exchange(nowUp);
    if (was == nowUp && everReported.exchange(true)) return;
    everReported = true;
    if (nowUp) {
        std::cerr << "[HMS-LINK] Connected to the HMS at " << client->address() << " as ADC-" << id.load()
                  << " (" << waiting() << " queued message(s) to send";
        if (droppedCount > 0) std::cerr << "; " << droppedCount << " older one(s) were dropped: queue full";
        std::cerr << ")\n";
    } else {
        std::cerr << "[HMS-LINK] HMS not reachable at " << client->address()
                  << ": dispensing continues locally; messages are queued and sent when it is back\n";
    }
}

void AdcLink::run() {
    Steady::time_point nextBeat = Steady::now();
    Steady::time_point stopBy{};
    while (true) {
        if (stopping && stopBy == Steady::time_point{}) stopBy = Steady::now() + std::chrono::seconds(3);
        if (stopping && (waiting() == 0 || !linkUp || Steady::now() > stopBy)) return;

        // A heartbeat when due; while the link is down it doubles as the retry probe.
        if (!stopping && Steady::now() >= nextBeat) {
            trySend(std::to_string(id.load()) + "|Heartbeat");
            nextBeat = Steady::now() + std::chrono::milliseconds(linkUp ? heartbeatMs : RETRY_MS);
        }
        // Queued messages, oldest first, while the link is up.
        while (linkUp) {
            std::string next;
            {
                std::lock_guard<std::mutex> guard(queueLock);
                if (queue.empty()) break;
                next = queue.front();
            }
            if (!trySend(next)) break;
            std::lock_guard<std::mutex> guard(queueLock);
            if (!queue.empty() && queue.front() == next) queue.pop_front();
        }
        std::unique_lock<std::mutex> lock(queueLock);
        wake.wait_until(lock, linkUp ? nextBeat : std::min(nextBeat, Steady::now() + std::chrono::milliseconds(RETRY_MS)),
                        [this] { return stopping.load() || (linkUp && !queue.empty()); });
    }
}
