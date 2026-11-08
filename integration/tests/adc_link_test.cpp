// Tests for the ADC side's link and console observer (integration/adc_side/
// adc_link.*, console_observer.*). The HMS is a small stand-in server in this
// test that decrypts packets with the same key; it can be down, up, or cut (503).

#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "adc_side/adc_link.hpp"
#include "adc_side/console_observer.hpp"
#include "tests/fake_hms.hpp"

static int passed = 0;
static const int PORT = 19041;

static std::unique_ptr<AdcLink> linkTo(FakeHms& hms, int heartbeatSeconds = 1) {
    return std::make_unique<AdcLink>(3, std::make_unique<HmsClient>("127.0.0.1", PORT, hms.key), heartbeatSeconds);
}

// ---- Console observer ----

static void observer_passes_text_through_and_splits_lines() {
    std::ostringstream console;
    std::vector<std::string> lines;
    ConsoleObserver observer(console.rdbuf(), [&](const std::string& l) { lines.push_back(l); });
    std::ostream out(&observer);
    out << "first line\nsecond " << 42 << " part";
    out.put('\n');
    out << "unfinished";
    assert(console.str() == "first line\nsecond 42 part\nunfinished");
    assert(lines.size() == 2 && lines[0] == "first line" && lines[1] == "second 42 part");
    passed++;
}

static void observer_keeps_lines_whole_across_threads() {
    std::ostringstream console;
    std::vector<std::string> lines;
    ConsoleObserver observer(console.rdbuf(), [&](const std::string& l) { lines.push_back(l); });
    std::ostream out(&observer);
    auto writer = [&](char tag) {
        // One write per line. Two writes per line ("text" << "\n") could interleave
        // between threads: the observer can't fix that (REPORT_NOTES.md, R5).
        for (int i = 0; i < 200; i++) out << std::string(30, tag) + "\n";
    };
    std::thread a(writer, 'a'), b(writer, 'b');
    a.join();
    b.join();
    assert(lines.size() == 400);
    for (const std::string& l : lines) assert(l == std::string(30, 'a') || l == std::string(30, 'b'));
    passed++;
}

// ---- Link ----

static void hms_down_queues_and_ping_fails_then_everything_arrives_in_order() {
    FakeHms hms(PORT);
    std::unique_ptr<AdcLink> link = linkTo(hms);
    link->enqueue("3|Withdrawal|Tylenol|1|a");
    link->enqueue("3|Withdrawal|Tylenol|2|a");
    assert(!link->pingNow() && !link->up());
    assert(link->waiting() == 2);
    hms.start();   // the HMS comes up
    assert(within(4000, [&] { return hms.events().size() == 2; }));
    assert(hms.events()[0] == "3|Withdrawal|Tylenol|1|a" && hms.events()[1] == "3|Withdrawal|Tylenol|2|a");
    assert(link->up() && link->pingNow() && link->waiting() == 0);
    passed++;
}

static void heartbeats_flow_while_up() {
    FakeHms hms(PORT);
    hms.start();
    std::unique_ptr<AdcLink> link = linkTo(hms, 1);
    assert(within(3500, [&] { return hms.heartbeats() >= 3; }));
    passed++;
}

static void cut_link_queues_then_restore_flushes() {
    FakeHms hms(PORT);
    hms.start();
    std::unique_ptr<AdcLink> link = linkTo(hms);
    assert(within(2000, [&] { return link->up(); }));
    hms.cut = true;
    assert(!link->pingNow() && !link->up());
    link->enqueue("3|Withdrawal|Morphine|5|nurse_jane");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    assert(hms.events().empty() && link->waiting() == 1);
    hms.cut = false;
    assert(within(4000, [&] { return hms.events().size() == 1; }));
    passed++;
}

static void full_queue_drops_the_oldest() {
    FakeHms hms(PORT);
    std::unique_ptr<AdcLink> link = linkTo(hms);
    for (int i = 1; i <= 205; i++) link->enqueue("3|Withdrawal|Tylenol|" + std::to_string(i) + "|a");
    assert(link->waiting() == AdcLink::QUEUE_LIMIT && link->dropped() == 5);
    hms.start();
    assert(within(8000, [&] { return hms.events().size() == AdcLink::QUEUE_LIMIT; }));
    assert(hms.events().front() == "3|Withdrawal|Tylenol|6|a" && hms.events().back() == "3|Withdrawal|Tylenol|205|a");
    passed++;
}

static void stopping_sends_what_is_queued() {
    FakeHms hms(PORT);
    hms.start();
    std::unique_ptr<AdcLink> link = linkTo(hms, 60);
    assert(within(2000, [&] { return link->up(); }));
    link->enqueue("3|Deposit|Morphine|5|pharm_bob");
    link.reset();   // the ADC shutting down
    assert(hms.events().size() == 1);
    passed++;
}

static void stopping_with_the_hms_down_does_not_hang() {
    FakeHms hms(PORT);   // never started
    std::unique_ptr<AdcLink> link = linkTo(hms);
    link->enqueue("3|Deposit|Morphine|5|pharm_bob");
    auto start = std::chrono::steady_clock::now();
    link.reset();
    assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
    passed++;
}

int main() {
    observer_passes_text_through_and_splits_lines();
    observer_keeps_lines_whole_across_threads();
    hms_down_queues_and_ping_fails_then_everything_arrives_in_order();
    heartbeats_flow_while_up();
    cut_link_queues_then_restore_flushes();
    full_queue_drops_the_oldest();
    stopping_sends_what_is_queued();
    stopping_with_the_hms_down_does_not_hang();
    std::cout << "adc_link_test: " << passed << " tests passed\n";
    return 0;
}
