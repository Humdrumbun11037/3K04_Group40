//history reader (interface only)

/*
Owned by history storage and audit trail (software decisions). This file only
states what the behaviour modules need to read back: every entry in a time window.
Kept separate from History so modules that only write do not depend on reading.

Interface
takes: a time window [from, to)
sends: the entries in it, oldest first, or a failure
*/
#pragma once

#include <vector>

#include "behaviour_hiding/interfaces/history.hpp"

namespace hms::behaviour {

enum class ReadVerdict { Ok, StorageFailure };

struct HistoryReadResult {
    ReadVerdict verdict;
    std::vector<HistoryEntry> entries;   // empty unless verdict == Ok
};

class HistoryReader {
public:
    virtual ~HistoryReader() = default;

    // { Pre: none }
    //   read(from, to)
    // { Post: Ok             => every stored entry with from <= time < to, oldest first
    //         StorageFailure => no entries }
    virtual HistoryReadResult read(long long from, long long to) const = 0;
};

}
