//fake history

/*
Keeps entries in a list instead of storing them. Implements both History
(write) and HistoryReader (read), so what a test writes it can read back.
Can be told to fail: failWrites(true) makes every record() return
StorageFailure, failReads(true) makes every read() fail, until set back to false.
*/
#pragma once

#include <vector>

#include "behaviour_hiding/interfaces/history.hpp"
#include "behaviour_hiding/interfaces/history_reader.hpp"

class FakeHistory : public hms::behaviour::History, public hms::behaviour::HistoryReader {
public:
    hms::behaviour::HistoryResult record(const hms::behaviour::HistoryEntry& entry) override {
        if (writesFail) return hms::behaviour::HistoryResult::StorageFailure;
        entries.push_back(entry);
        return hms::behaviour::HistoryResult::Stored;
    }

    hms::behaviour::HistoryReadResult read(long long from, long long to) const override {
        if (readsFail) return {hms::behaviour::ReadVerdict::StorageFailure, {}};
        std::vector<hms::behaviour::HistoryEntry> found;
        for (const auto& entry : entries) {
            if (from <= entry.time && entry.time < to) found.push_back(entry);
        }
        return {hms::behaviour::ReadVerdict::Ok, found};
    }

    void failWrites(bool fail) { writesFail = fail; }
    void failReads(bool fail) { readsFail = fail; }

    std::vector<hms::behaviour::HistoryEntry> entries;   // in the order written

private:
    bool writesFail = false;
    bool readsFail = false;
};
