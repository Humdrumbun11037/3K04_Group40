//integrated: Matthew's encrypted storage as Andrew's history

/*
Hides: how a HistoryEntry is kept in Matthew's FileAuditTrail (encrypted file)
  record(entry)    -> FileAuditTrail::append(Event)
  read(from, to)   -> FileAuditTrail::search(text from, text to-1)
Converts:
  - time:  seconds <-> "YYYY-MM-DD HH:MM:SS"; Andrew reads [from, to), Matthew
           searches from <= t <= to, so the end is moved back one second
  - ADC:   0 <-> -1
  - kind:  Inodh's EventType has no general-purpose value, so the event type
           is only a carrier (an exact name match if there is one, else the
           closest category) and the real kind, who, detail and medication
           travel in the event's fields
  - errors: Matthew's storage throws; every exception becomes StorageFailure,
           so Andrew's modules keep their "changed only if recorded" rules
The failure switches are the demo's simulated failures, on top of real ones.
*/
#pragma once

#include "gui/integrated/convert.hpp"

#include "behaviour_hiding/interfaces/history.hpp"
#include "behaviour_hiding/interfaces/history_reader.hpp"

class FileHistory : public hms::behaviour::History, public hms::behaviour::HistoryReader {
public:
    explicit FileHistory(FileAuditTrail& trail) : trail(trail) {}

    hms::behaviour::HistoryResult record(const hms::behaviour::HistoryEntry& entry) override;
    hms::behaviour::HistoryReadResult read(long long from, long long to) const override;

    void failWrites(bool fail) { writesFail = fail; }
    void failReads(bool fail) { readsFail = fail; }

    mutable int realFailures = 0;   // exceptions from the storage itself (not the simulated switch)
    mutable std::string lastError;

private:
    FileAuditTrail& trail;
    bool writesFail = false;
    bool readsFail = false;
};
