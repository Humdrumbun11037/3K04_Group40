# HMS behaviour-hiding modules: module notes

Behaviour-hiding modules of the Hospital Monitoring System (HMS), Subgroup B,
owned by Andrew Gittens. Each module hides one rule set that would change if a
requirement were rewritten.

Notation used throughout:

- Types that come from teammates' interfaces (`AdcId`, `User`, `Role`, `Clock`,
  `History`, `HistoryEntry`, `HistoryReader`, `HeartbeatCounter`, `AdcRegistry`,
  `ReportSender`, `AlarmNotifier`, `Permissions`, `Action`, `MaintenanceStatus`)
  live in namespace `hms::behaviour`. The signatures below omit that prefix; the
  headers spell it out.
- An ADC is identified by a positive `int` (`AdcId`). 0 is never a real ADC, so it
  means "no ADC" in a history entry and "any ADC" in a filter. An alarm or record
  for the whole hospital (Hospital stock) also uses 0. Wherever an ADC is shown to
  people it is written `ADC-<n>`, and 0 is written `Hospital` (the helper
  `adcName` in `adc_id.hpp`); a summary filter of 0 is still shown as `any`.
- **No module contains a role rule.** Every "may this user do this?" question is
  asked of the `Permissions` interface, so changing who may do what changes only
  the permission table.
- Times are `long long` seconds since 1970-01-01 00:00 **hospital local time**
  (team assumption: no time-zone handling anywhere in the HMS).
- **No operation throws a C++ exception.** Every invalid input is answered with
  a rejection verdict (an `enum class` value), and a rejected call changes
  nothing. The "Exceptions" rows below list those verdicts.
- Function tables are checked in order; the first matching row decides, which
  makes each table disjoint. Each table appears in the source as one block with
  `// row N` comments, and each row has at least one test named after it.

## Module overview

| # | Module | Files | Class | Uses |
|---|--------|-------|-------|------|
| 1 | Alarm severity | `behaviour_hiding/alarm_severity.hpp/.cpp` | `AlarmSeverity` | nothing |
| 2 | Low stock rule | `behaviour_hiding/low_stock_rule.hpp/.cpp` | `LowStockRule` | nothing |
| 3 | Maintenance | `behaviour_hiding/maintenance.hpp/.cpp` | `Maintenance` (implements `MaintenanceStatus`) | Clock, AdcRegistry, History, Permissions |
| 4 | ADC heartbeats | `behaviour_hiding/adc_heartbeats.hpp/.cpp` | `AdcHeartbeats` | Clock, MaintenanceStatus, AlarmNotifier, History |
| 5 | Inventory record | `behaviour_hiding/inventory_record.hpp/.cpp` | `InventoryRecord` | Low stock rule, Clock, History, AlarmNotifier, Permissions |
| 6 | Alarm resolution | `behaviour_hiding/alarm_resolution.hpp/.cpp` | `AlarmResolution` | Alarm severity, Clock, History, Permissions |
| 7 | Report types | `behaviour_hiding/report_types.hpp/.cpp` | `ReportTypes` | Clock, History, HistoryReader, HeartbeatCounter, AdcRegistry, ReportSender, Permissions |
| 8 | Report formatting | `behaviour_hiding/report_formatting.hpp/.cpp` | `ReportFormatting` | Report types (data types only) |
| 9 | Hospital stock | `behaviour_hiding/hospital_stock.hpp/.cpp` | `HospitalStock` | Low stock rule, Inventory record (reads only), Clock, History, AlarmNotifier, Permissions |

The uses relation is acyclic: 1 and 2 use nothing; 3 uses only interfaces;
4 uses 3 through the `MaintenanceStatus` interface; 5 uses 2; 6 uses 1; 8 uses
7's data types; 7 uses only interfaces; 9 uses 2 and 5. Inventory record does
not use Hospital stock, so 5 and 9 do not form a cycle.

**Place in the team's module tree** (`module_diagram.txt`): Hospital stock
belongs under Inventory management (`behav_hiding/inventory_manager/`), next to
inventory record and low stock rule. The tree file is outside `Subgroup_B`,
so it still needs adding there.

---

## 1. Alarm severity

| | |
|---|---|
| **Responsibility** | Give the severity of each alarm type (FR-ALM-03). |
| **Secret** | The type-to-severity table: Unauthorized access, ADC offline and Incorrect message format are Critical; Inventory low, Hospital low stock and Inventory discrepancy are High; Connection loss is Low. |
| **Exported types** | `enum class AlarmType { UnauthorizedAccess, AdcOffline, IncorrectMessageFormat, InventoryLow, ConnectionLoss, HospitalLowStock, InventoryDiscrepancy }`; `enum class Severity { Low, High, Critical }` (ordered lowest to highest); `enum class SeverityVerdict { Ok, UnknownType }`; `struct SeverityResult { SeverityVerdict verdict; Severity severity; }` |
| **Assumptions** | None beyond the table. |
| **Effects** | None (no state). |
| **Exceptions** | `UnknownType` for a value not in the table; the severity field is then Critical, so a caller that ignores the verdict errs on the safe side. |

```cpp
SeverityResult severityOf(AlarmType type) const;
```

**Semantics.** Pre: none. Post: type in the table, so `{Ok, severity from the
table}`; otherwise `{UnknownType, ...}`, never guessed.

| # | Alarm type | Severity |
|---|------------|----------|
| 1 | UnauthorizedAccess | Critical |
| 2 | AdcOffline | Critical |
| 3 | IncorrectMessageFormat | Critical |
| 4 | InventoryLow | High |
| 5 | ConnectionLoss | Low |
| 6 | HospitalLowStock | High |
| 7 | InventoryDiscrepancy | High |

---

## 2. Low stock rule

| | |
|---|---|
| **Responsibility** | Decide whether a medication count is low stock (FR-INV-02). |
| **Secret** | The threshold (25%, one named constant), that it is compared against a capacity, and that exactly 25% is not low. Inventory record applies it to one medication in one ADC; hospital stock applies it to one medication across the hospital. |
| **Exported types** | `enum class LowStockVerdict { NotLow, Low, InvalidCapacity, InvalidCount }` |
| **Assumptions** | Evaluated per medication, for one ADC or for the whole hospital; capacity is supplied by the caller. Whole-number arithmetic (`count * 100 < capacity * 25`) avoids rounding. |
| **Effects** | None (no state). |
| **Exceptions** | `InvalidCapacity` (capacity <= 0), `InvalidCount` (count < 0). |

```cpp
LowStockVerdict check(int count, int capacity) const;
```

**Semantics.** Pre: none. Post, first matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | capacity <= 0 | `InvalidCapacity` |
| 2 | count < 0 | `InvalidCount` |
| 3 | count strictly below 25% of capacity (0 included) | `Low` |
| 4 | otherwise (exactly 25% or more) | `NotLow` |

---

## 3. Maintenance

| | |
|---|---|
| **Responsibility** | Hold maintenance windows per ADC; say whether an ADC is in maintenance at a given time (FR-HLT-01, FR-HLT-02). |
| **Secret** | How windows are stored (a list); their lifecycle (an enum plus a transition table: Scheduled to Active at start, Active to Completed at end, Scheduled or Active to Cancelled); the rules for accepting and cancelling a window, including the 12-hour maximum (one named constant). |
| **Exported types** | `enum class ScheduleVerdict { Created, NotAuthorized, UnknownAdc, InvalidWindow, WindowTooLong, Overlap }`; `enum class CancelVerdict { Cancelled, NotAuthorized, UnknownWindow, NotCancellable }` |
| **State invariant** | No two open (Scheduled or Active) windows for one ADC overlap. Established by the constructor (no windows) and kept by `schedule()`. |
| **Assumptions** | Windows are half-open `[start, end)`, so back-to-back windows are allowed. A window may start in the past if it ends in the future. Completed and Cancelled windows do not count for overlap. A window of exactly 12 hours is allowed. Who may schedule or cancel comes from `Permissions` (agreed table: Technician only). |
| **Effects** | `schedule` adds a window; `cancel` cancels one; every operation first brings window states up to date with the time. Each schedule, start, completion and cancellation writes one history entry: kind `Maintenance`, the ADC filled in, detail `"<adc> window scheduled|started|completed|cancelled (start=S end=E)"`. Who is the user for schedule and cancel, `HMS` for start and completion. Start and completion entries are timed at the window's own start and end. A failed history write is ignored and never blocks a change. |
| **Exceptions** | The rejection verdicts listed below. |

```cpp
Maintenance(const Clock& clock, const AdcRegistry& registry, History& history,
            const Permissions& permissions);
ScheduleVerdict schedule(const User& user, const AdcId& adc, long long start, long long end);
CancelVerdict cancel(const User& user, const AdcId& adc, long long start);
bool isInMaintenance(const AdcId& adc, long long now) override;
```

**`schedule`.** Pre: none. Post, first matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | `Permissions` denies `ScheduleMaintenance` for the user's role | `NotAuthorized` |
| 2 | ADC unregistered or decommissioned | `UnknownAdc` |
| 3 | end <= start, or end <= clock now | `InvalidWindow` |
| 4 | end - start > 12 hours | `WindowTooLong` |
| 5 | overlaps an open window for that ADC | `Overlap` |
| 6 | otherwise | `Created`, window added as Scheduled, history entry "scheduled" |

**`cancel`.** Pre: none. Post, first matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | `Permissions` denies `CancelMaintenance` for the user's role | `NotAuthorized` |
| 2 | the ADC has no window starting at `start` | `UnknownWindow` |
| 3 | that window is Completed or Cancelled | `NotCancellable` |
| 4 | otherwise (Scheduled or Active) | `Cancelled`; if it was Active, suppression ends immediately; history entry "cancelled" |

**`isInMaintenance`.** Pre: `now` is not earlier than any time asked about
before. Post: window states are brought up to date for `now`, with a history
entry for each start and completion; returns true if and only if the ADC has an
Active window.

---

## 4. ADC heartbeats

| | |
|---|---|
| **Responsibility** | Track missed reporting periods, mark an ADC Offline, raise the communication-loss alarm (FR-COM-04, FR-HLT-02, NFR-SAF-01). |
| **Secret** | The offline rule: 60 s heartbeat period and 3 missed periods, two named constants. How silence is tracked: the time each ADC was last heard from, rather than a counter. |
| **Exported types** | `enum class AdcStatus { Online, Offline, NotMonitored }`; `enum class HeartbeatVerdict { Accepted, NotMonitored }` |
| **State invariants** | An ADC that is not monitored (never added, or decommissioned) never raises an alarm. At most one alarm per offline spell. A silent ADC in maintenance is Offline but raises no alarm until the window ends. This module never sends anything to an ADC, so it cannot affect dispensing. A history write never blocks a status change or an alarm. |
| **Assumptions** | The alarm raised is `AdcOffline` (Critical); Inodh to confirm. An ADC counts as heard from when monitoring starts. Message validation is done before `heartbeatReceived` is called. Something outside this module calls `checkAll()` regularly. |
| **Effects** | Updates ADC statuses; calls `AlarmNotifier::raise(adc, AlarmType::AdcOffline)`. Each change to Offline or back to Online writes one history entry: kind `AdcHealth`, who `HMS`, the ADC filled in, detail `"<adc> offline: 3 heartbeat periods missed"`, `"<adc> offline during maintenance: no alarm"` or `"<adc> back online"`. It is written after the change and the alarm; a failed write is ignored. |
| **Exceptions** | `HeartbeatVerdict::NotMonitored` for a heartbeat from an unmonitored ADC. |

```cpp
AdcHeartbeats(const Clock& clock, MaintenanceStatus& maintenance, AlarmNotifier& notifier,
              History& history);
void monitor(const AdcId& adc);
void stopMonitoring(const AdcId& adc);
HeartbeatVerdict heartbeatReceived(const AdcId& adc);
void checkAll();
AdcStatus status(const AdcId& adc) const;
```

- **`monitor`.** Pre: the ADC is registered. Post: the ADC is monitored, Online,
  and heard from now. If it was already monitored, nothing changes.
- **`stopMonitoring`.** Post: the ADC is not monitored and never raises an alarm here.
- **`heartbeatReceived`.** Spec row 1. A monitored ADC gets its missed count
  reset, goes Online, and the result is `Accepted`; if it was Offline, a
  "back online" entry is written. Otherwise `NotMonitored` and nothing changes.
- **`status`.** Returns Online or Offline, or NotMonitored if the ADC is not monitored.
- **`checkAll`.** Applies this table to each monitored ADC at clock now, first
  matching row:

| # | Condition | Result | Spec row |
|---|-----------|--------|----------|
| A | fewer than 3 periods missed | no change | |
| B | 3 or more missed, in maintenance | Offline (entry if it was Online), no alarm | 3 |
| C | 3 or more missed, alarm already raised this spell | no change | FR-ALM-04 |
| D | 3 or more missed, not in maintenance, no alarm yet | Offline, raise `AdcOffline` (entry if it was Online) | 2 and 4 |

---

## 5. Inventory record

| | |
|---|---|
| **Responsibility** | Keep medication counts; apply withdrawals, deposits and authorized manual changes; record a withdrawal larger than the count as a discrepancy (FR-INV-01, FR-INV-03, FR-INV-04, NFR-PRF-02). |
| **Secret** | How counts are stored (count and capacity per ADC and medication) and which changes are valid. |
| **Exported types** | `enum class AddItemVerdict { Added, AlreadyExists, IncorrectValue }`; `enum class InventoryVerdict { Updated, NotAuthorized, UnknownItem, IncorrectValue, NoExplanation, StorageFailure, DiscrepancyRecorded }`; `struct StockItem { AdcId adc; std::string medication; int count; int capacity; }` |
| **State invariants** | Every count is 0 or more and every capacity is more than 0. A count changes only if the change was stored in history. |
| **Assumptions** | A count of 0 is valid. A withdrawal or deposit must be more than 0. A withdrawal larger than the count is not rejected: the ADC has already dispensed, so it is recorded, the count goes to 0 and a discrepancy alarm is raised. Counts above capacity are allowed, from a deposit or a manual change. `addItem` writes no history and raises no alarm. Who may make a manual change comes from `Permissions` (agreed table: Pharmacist or Administrator). Updates happen within the call, which meets NFR-PRF-02 (3 s). |
| **Effects** | Changes counts; writes `InventoryWithdrawal`, `InventoryDiscrepancy`, `InventoryDeposit` and `InventoryManualChange` entries to history (with ADC and medication; the daily report puts them all in Inventory changes). Calls `AlarmNotifier::raise(adc, AlarmType::InventoryDiscrepancy, medication)` for an over-withdrawal, and `AlarmNotifier::raise(adc, AlarmType::InventoryLow, medication)` after every accepted change that leaves the count low. Repeats for the same ADC and medication join the open alarm. A discrepancy alarm is resolved with a note like any other alarm. |
| **Exceptions** | The rejection verdicts listed below. |

```cpp
InventoryRecord(const Clock& clock, History& history, AlarmNotifier& notifier,
                const Permissions& permissions);
AddItemVerdict addItem(const AdcId& adc, const std::string& medication, int count, int capacity);
InventoryVerdict withdraw(const AdcId& adc, const std::string& medication, int amount);
InventoryVerdict deposit(const AdcId& adc, const std::string& medication, int amount);
InventoryVerdict manualChange(const User& user, const AdcId& adc, const std::string& medication,
                              int newCount, const std::string& explanation);
std::optional<int> count(const AdcId& adc, const std::string& medication) const;
std::vector<StockItem> items() const;
```

**`addItem`.**

| Condition | Result |
|-----------|--------|
| adc <= 0, count < 0, or capacity <= 0 | `IncorrectValue` |
| already tracked | `AlreadyExists` |
| otherwise | `Added` |

**`withdraw`.** Pre: the ADC upload was validated. Post, first matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | item not tracked | `UnknownItem` |
| 2 | amount <= 0 | `IncorrectValue` |
| 3 | history write fails | `StorageFailure` |
| 4 | amount > count | `DiscrepancyRecorded`, count = 0, recorded as `InventoryDiscrepancy` (detail `withdrew N <med>: C -> 0 (discrepancy: HMS count was C)`); `InventoryDiscrepancy` alarm (detail = medication), then a low-stock alarm if now low |
| 5 | otherwise | `Updated`, count -= amount, recorded as `InventoryWithdrawal`; low-stock alarm (detail = medication) if now low |

Rows 1–3 change nothing and raise nothing.

**`deposit`.** Pre: the ADC upload was validated. Post, first matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | item not tracked | `UnknownItem` |
| 2 | amount <= 0, or the new count would not fit in an `int` | `IncorrectValue` |
| 3 | history write fails | `StorageFailure`, nothing changes |
| 4 | otherwise | `Updated`, count += amount (above capacity allowed), recorded as `InventoryDeposit` (detail `deposited N <med>: C -> C+N`); no alarm |

**`manualChange`.** Pre: none. Post, first matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | `Permissions` denies `ManualInventoryChange` for the user's role | `NotAuthorized` |
| 2 | item not tracked | `UnknownItem` |
| 3 | newCount < 0 | `IncorrectValue` |
| 4 | explanation empty or only whitespace | `NoExplanation` |
| 5 | history write fails | `StorageFailure` |
| 6 | otherwise | `Updated`, count = newCount, recorded with the explanation; low-stock alarm (detail = medication) if now low |

**Queries.** `count` returns the current count, or nothing if the item isn't
tracked. `items` returns a copy of every item, ordered by ADC then medication.

---

## 6. Alarm resolution

| | |
|---|---|
| **Responsibility** | Carry each alarm from Raised to Resolved; resolve only with a note; escalate unacknowledged alarms (FR-ALM-02, FR-ALM-04, FR-ALM-06, NFR-SAF-02, NFR-PRF-04). |
| **Secret** | The alarm lifecycle (an enum plus a transition table); that a note is required; the escalation limits (Critical 2 min, High 10 min, Low never, as named constants); the supervisor chain (Technician and Pharmacist to Administrator, Administrator none); one-level severity raise (Critical stays Critical). |
| **Exported types** | `using AlarmId = int`; `enum class AlarmState { Raised, Acknowledged, Resolved }`; `enum class AddVerdict { Added, JoinedOpenAlarm, UnknownType }`; `enum class AcknowledgeVerdict { Acknowledged, NotAuthorized, UnknownAlarm, NotRaised, StorageFailure }`; `enum class ResolveVerdict { Resolved, NotAuthorized, UnknownAlarm, AlreadyResolved, NoExplanation, StorageFailure }`; `struct AddResult { AddVerdict verdict; AlarmId id; }`; `struct AlarmInfo { AdcId adc; AlarmType type; std::string detail; Severity severity; Role destination; AlarmState state; bool escalated; long long raisedAt; }`; `struct Escalation { AlarmId id; Role destination; Severity severity; }` |
| **State invariants** | An alarm is Acknowledged or Resolved only if that action was stored in history. An alarm escalates at most once, and only while Raised. A Resolved alarm never changes again. Each ADC has at most one unresolved alarm per type and detail. |
| **Transition table** | T1 Raised + Acknowledge gives Acknowledged; T2 Acknowledged + Resolve gives Resolved; T3 Raised + Resolve gives Resolved (an implicit acknowledgement). Any other pair leaves the state unchanged. |
| **Assumptions** | Who may acknowledge and who may resolve are two separate questions to `Permissions` (agreed table: all three roles for both). Escalation changes the existing alarm rather than creating a new one (Inodh to confirm). An escalation still happens if its history write fails. Escalation fires at exactly the limit. Alarm notification calls `add` and chooses the destination role. An invalid action (for example acknowledging a resolved alarm) returns a verdict, changes nothing and writes no log entry, because logging every rejected action would flood the audit trail. This departs from the Week 6 slide ("no change plus a log entry") and is easy to change. |
| **Effects** | Writes `AlarmAcknowledged`, `AlarmResolved` and `AlarmEscalated` entries to history (with the ADC). The detail starts `alarm <id> (<adc>)`, for example `alarm 3 (ADC-1)` or `alarm 4 (Hospital)`, so report lines say where the alarm was; a resolution adds `: <note>`. |
| **Exceptions** | The rejection verdicts listed below. |

```cpp
AlarmResolution(const Clock& clock, History& history, const Permissions& permissions);
AddResult add(const AdcId& adc, AlarmType type, Role destination, const std::string& detail = "");
AcknowledgeVerdict acknowledge(const User& user, AlarmId id);
ResolveVerdict resolve(const User& user, AlarmId id, const std::string& note);
std::vector<Escalation> escalateDue();
std::optional<AlarmInfo> find(AlarmId id) const;
```

**`add`.** First matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | type not in the severity table | `UnknownType` |
| 2 | the ADC already has an unresolved alarm with this type and detail | `JoinedOpenAlarm` with that alarm's id |
| 3 | otherwise | `Added`: new id, Raised, detail kept, severity from Alarm severity, raised at clock now |

**`acknowledge`.** First matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | `Permissions` denies `AcknowledgeAlarm` for the user's role | `NotAuthorized` |
| 2 | no such alarm | `UnknownAlarm` |
| 3 | alarm is not Raised | `NotRaised` |
| 4 | history write fails | `StorageFailure` (the alarm stays Raised and can still escalate) |
| 5 | otherwise | `Acknowledged`, recorded; it will not escalate |

**`resolve`.** First matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | `Permissions` denies `ResolveAlarm` for the user's role | `NotAuthorized` |
| 2 | no such alarm | `UnknownAlarm` |
| 3 | already Resolved | `AlreadyResolved` |
| 4 | note empty or only whitespace | `NoExplanation` |
| 5 | history write fails | `StorageFailure`, not resolved |
| 6 | otherwise | `Resolved`; history records who and why |

**`escalateDue`.** Applies to every alarm that is Raised, not yet escalated, and
at or past its severity's limit:

| # | Condition | Result |
|---|-----------|--------|
| 1 | destination has a supervisor | rerouted to the supervisor |
| 2 | no supervisor, not Critical | severity one level higher |
| 3 | no supervisor, Critical | stays Critical (still marked escalated) |

It returns the escalations made, so alarm notification can deliver them.

**`find`.** Returns a copy of the alarm's details, or nothing.

---

## 7. Report types

| | |
|---|---|
| **Responsibility** | Decide what the daily report and the activity summary contain, and which filters apply (FR-REP-01, FR-REP-02, NFR-TRC-01). No layout. |
| **Secret** | When the daily report is due (06:00, once per local day); its window (previous 24 hours); which history entry goes in which section (by the start of its kind: `Inventory…` and `ReserveChange` to Inventory changes, `Alarm…` to Alarms, `AdcHealth` and `Maintenance` to ADC health, anything else to Events); when an ADC needs attention (fewer heartbeats than one per 60 s); the audit rule. Who may request a summary comes from `Permissions` (agreed table: Pharmacist or Administrator). |
| **Exported types** | `struct FlaggedAdc { AdcId adc; int received; int expected; }`; `struct DailyReport { long long from, to; std::vector<FlaggedAdc> adcsNeedingAttention; std::vector<HistoryEntry> inventoryChanges, events, alarms, adcHealth; }`; `enum class DailyVerdict { Sent, AlreadyDone, TooEarly, GenerationFailed, SendFailed }`; `struct SummaryFilters { AdcId adc; std::string medication; std::string actor; long long from; long long to; }` (adc 0, or an empty medication or actor, means any); `struct Summary { SummaryFilters filters; std::vector<HistoryEntry> entries; }`; `enum class SummaryVerdict { Summarized, NotAuthorized, InvalidRange, ReadFailure, AuditFailure }`; `struct SummaryResult { SummaryVerdict verdict; Summary summary; }` |
| **State invariants** | At most one daily report is sent per local day. A day is marked done only after a successful send. Every summary request writes exactly one audit entry. |
| **Assumptions** | Local day = floor(seconds / 86400); 06:00 = 21600 s into the day. The daily window is `[now - 24 h, now)`; the summary window is `[from, to]`, both ends included. Decommissioned ADCs are not flagged. ADCs in maintenance are still flagged. |
| **Effects** | Sends the daily report to `ReportSender`. Writes `ReportFailed` entries on failure and one `SummaryRequested` audit entry per summary request. Generating the report itself only reads. |
| **Exceptions** | The verdicts listed below. |

```cpp
ReportTypes(const Clock& clock, const HistoryReader& reader, History& history,
            const HeartbeatCounter& heartbeats, const AdcRegistry& registry, ReportSender& sender,
            const Permissions& permissions);
DailyVerdict generateDaily(long long now);
SummaryResult summary(const User& user, const SummaryFilters& filters);
```

**`generateDaily`.** Pre: called regularly (every tick). Post, first matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | today's report was already sent | `AlreadyDone`, no action |
| 2 | now is before 06:00 | `TooEarly`, no action |
| 3 | reading history or heartbeat counts fails | `GenerationFailed`; logged; retried on the next call |
| 4 | sending fails | `SendFailed`; logged; retried on the next call |
| 5 | otherwise | `Sent`: one report for `[now - 24 h, now)`, and today is marked done. An empty window still gives a report with empty sections. |

**`summary`.** First matching row decides the verdict, then exactly one audit
entry is written. If that audit write fails, the result is `AuditFailure` with
no entries.

| # | Condition | Result | Audit |
|---|-----------|--------|-------|
| 1 | `Permissions` denies `GenerateSummary` for the user's role | `NotAuthorized` | written |
| 2 | from > to | `InvalidRange` | written |
| 3 | reading history fails | `ReadFailure` | written |
| 4 | otherwise | `Summarized`: every entry in `[from, to]` matching every filter given (possibly none) | written |

---

## 8. Report formatting

| | |
|---|---|
| **Responsibility** | Turn report data into plain text. |
| **Secret** | Layout: section order and titles, line format, and how seconds become a local date and time. The date conversion is done by hand (leap years included) with no time-zone library. ADCs are shown as `ADC-<n>`, and ADC 0 as `Hospital`. |
| **Assumptions** | Seconds are hospital local time since 1970-01-01. Delivery is done by the `ReportSender`, which uses this module to get the text. |
| **Effects** | None (no state). |
| **Exceptions** | None. Every input gives text. |

```cpp
std::string formatTime(long long seconds) const;
std::string formatDaily(const DailyReport& report) const;
std::string formatSummary(const Summary& summary) const;
```

- **`formatTime`.** Returns `YYYY-MM-DD HH:MM:SS`. Negative seconds (before
  1970) are handled.
- **`formatDaily`.** Returns `DAILY REPORT`, a `Window: <from> to <to>` line, a
  blank line, then the sections ADCs needing attention, Inventory changes,
  Events, Alarms and ADC health. Each section title carries its count, and an
  empty section shows `(none)`. Entry lines are indented and read
  `date time who kind detail`.
- **`formatSummary`.** Returns `ACTIVITY SUMMARY`, a filter line
  (`adc=ADC-<n> medication=… actor=… from=… to=…`, with `any` for an empty filter),
  `Entries: N`, then one line per entry: `date time who kind detail`.

---

## 9. Hospital stock

| | |
|---|---|
| **Responsibility** | Keep the hospital's active inventory of each medication across the pharmacy reserves and every ADC (FR-INV-01). Say when a medication is low across the hospital, using the same 25% rule as for one ADC applied across all ADCs plus the reserves (FR-INV-02), and raise a hospital-wide alarm for it. Accept authorized manual changes to a reserve. |
| **Secret** | How the hospital totals are calculated: reserve count and capacity plus the sum of every ADC's count and capacity for that medication, with the low stock rule applied to the totals. Where reserves are held: one count and capacity per medication, kept inside this module (the pharmacy stores are treated as one store). |
| **Place in the module tree** | Inventory management (`behav_hiding/inventory_manager/`), with inventory record and low stock rule. |
| **Exported types** | `enum class HospitalStockVerdict { NotLow, Low, UnknownMedication }`; `enum class ReserveVerdict { Updated, NotAuthorized, UnknownMedication, IncorrectValue, NoExplanation, StorageFailure }`; `struct HospitalStockLevel { std::string medication; int reserveCount; int reserveCapacity; int adcCount; int adcCapacity; int totalCount; int totalCapacity; HospitalStockVerdict verdict; }`; `static constexpr AdcId HOSPITAL = 0`. `addReserve` reuses `AddItemVerdict` from inventory record. |
| **State invariants** | Every reserve count is 0 or more and every reserve capacity is more than 0. A reserve count changes only if the change was stored in history. |
| **Assumptions** | A medication may be held only in reserve, only in ADCs, or both. ADC counts are read from inventory record on every call, so the totals always follow withdrawals, deposits and manual changes. Reserves are set up with `addReserve` (no history, like `addItem`); the demo's levels are made up. A reserve count above its capacity is allowed, as for ADCs. **Permission choice:** a manual reserve change is the same kind of act as a manual ADC change, so it asks `Permissions` for the existing `ManualInventoryChange` action (agreed table: Pharmacist or Administrator) instead of adding a new action. |
| **Effects** | `manualReserveChange` writes a `ReserveChange` history entry: who is the user, ADC 0, the medication, detail `reserve <med>: <before> -> <after>: <explanation>`. The daily report puts it in Inventory changes. `checkAll` calls `AlarmNotifier::raise(0, AlarmType::HospitalLowStock, medication)` only when a medication changes from not low to low between checks. While it stays low nothing more is raised, even after its alarm is resolved; it must be seen not low by a check before it can raise again. The module remembers which medications were low at the last check. `manualReserveChange` raises no alarm itself; the HMS main loop calls `checkAll` on every tick and after each inventory change. |
| **Exceptions** | The rejection verdicts listed below; `UnknownMedication` from `check`. |

```cpp
HospitalStock(const Clock& clock, const InventoryRecord& adcs, History& history,
              AlarmNotifier& notifier, const Permissions& permissions);
AddItemVerdict addReserve(const std::string& medication, int count, int capacity);
ReserveVerdict manualReserveChange(const User& user, const std::string& medication, int newCount,
                                   const std::string& explanation);
HospitalStockLevel check(const std::string& medication) const;
std::vector<HospitalStockLevel> all() const;
std::vector<std::string> checkAll();
```

**`addReserve`.**

| Condition | Result |
|-----------|--------|
| count < 0, or capacity <= 0 | `IncorrectValue` |
| reserve already tracked | `AlreadyExists` |
| otherwise | `Added` |

**`manualReserveChange`.** Pre: none. Post, first matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | `Permissions` denies `ManualInventoryChange` for the user's role | `NotAuthorized` |
| 2 | no reserve for the medication | `UnknownMedication` |
| 3 | newCount < 0 | `IncorrectValue` |
| 4 | explanation empty or only whitespace | `NoExplanation` |
| 5 | history write fails | `StorageFailure` |
| 6 | otherwise (0 included) | `Updated`, reserve count = newCount, recorded with the explanation |

**`check`.** Pre: none. Post: the totals are the reserve (if any) plus every ADC
holding the medication; then, first matching row:

| # | Condition | Verdict |
|---|-----------|---------|
| 1 | no reserve and no ADC holds it | `UnknownMedication` (all numbers 0) |
| 2 | total count strictly below 25% of total capacity | `Low` |
| 3 | otherwise | `NotLow` |

**`all`.** `check` for every medication found in the reserves or in any ADC,
ordered by name.

**`checkAll`.** Post, for each medication, first matching row:

| # | Condition | Result |
|---|-----------|--------|
| 1 | not Low now | no alarm; remembered as not low |
| 2 | Low now, and Low at the previous check | no alarm (no repeat, even if the alarm was resolved) |
| 3 | Low now, and not Low at the previous check (or never checked) | `HospitalLowStock` raised (ADC 0, detail = medication); remembered as low |

Returns the medications alarmed by this call, in name order. Recovery is only
noticed by a check: a medication that recovers and drops again between two
checks counts as staying low.

**How this differs from the per-ADC rule (and from Inodh's spec).** The spec,
and Inodh's `sw_decisions` code, only know stock per ADC. Inodh's
`InventoryStore` holds a count per ADC and medication, with no capacity and no
low-stock check at all. Inventory record keeps the per-ADC rule (`InventoryLow`
for one ADC). Hospital stock adds the hospital view on top, and the two can
disagree in both directions:

- **One ADC low, hospital not low.** The reserves cover it. Only the ADC alarm
  is raised.
- **Every ADC at 25% or more, hospital low.** The reserves are nearly empty.
  Only the hospital alarm is raised.

Both cases are tested.

**Not implemented** (deliberately; to agree with the team first):

- **Deliveries into the reserves** (stock arriving at the pharmacy). Today a
  delivery can only be recorded as a manual reserve change.
- **Transfers from the reserves into an ADC** (restocking an ADC), which would
  lower the reserve and raise the ADC count together.

ADC deposits are implemented, in inventory record (`deposit`). They raise the
ADC count only and do not take stock out of the reserves; linking the two is
the transfer above.

---

## Interfaces used that a teammate owns

All live in `behaviour_hiding/interfaces/`, namespace `hms::behaviour`. Each
abstract class has a fake in `tests/fakes/` that the tests and the demo use
(`User`, `Role`, `Action` and `AdcId` are plain types and need none). Every signature is a proposal to be agreed with the owner.

| Interface | Proposed owner | Operations | Used by |
|-----------|----------------|------------|---------|
| `Clock` | Hardware hiding | `virtual long long now() const = 0;` | 3, 4, 5, 6, 7, 9 |
| `AlarmNotifier` | Alarm notification (Inodh) | `virtual void raise(const AdcId& adc, AlarmType type, const std::string& detail = "") = 0;` (empty detail means none; Inventory record and Hospital stock pass the medication, for `InventoryLow`, `InventoryDiscrepancy` and `HospitalLowStock`; Hospital stock raises for ADC 0) | 4, 5, 9 |
| `AdcRegistry` | ADC registry | `virtual bool isActive(const AdcId& adc) const = 0;` and `virtual std::vector<AdcId> activeAdcs() const = 0;` | 3, 7 |
| `History` | History storage and audit trail | `virtual HistoryResult record(const HistoryEntry& entry) = 0;` | 3, 4, 5, 6, 7, 9 |
| `HistoryReader` | History storage and audit trail | `virtual HistoryReadResult read(long long from, long long to) const = 0;` (entries with from <= time < to) | 7 |
| `HeartbeatCounter` | ADC comms link or history (to agree) | `virtual HeartbeatCountResult count(const AdcId& adc, long long from, long long to) const = 0;` | 7 |
| `ReportSender` | Dashboards or messaging (to agree) | `virtual SendResult send(const DailyReport& report) = 0;` | 7 |
| `Permissions` | User settings (Inodh) | `virtual bool mayPerform(Role role, Action action) const = 0;` with `enum class Action { ScheduleMaintenance, CancelMaintenance, ManualInventoryChange, AcknowledgeAlarm, ResolveAlarm, GenerateSummary };` | 3, 5, 6, 7, 9 |
| `User`, `Role` | User information and login | `struct User { std::string name; Role role; };` and `enum class Role { Pharmacist, Technician, Administrator };` | 3, 5, 6, 7, 9 |
| `AdcId` | ADC registry | `using AdcId = int;` (positive; 0 = none, any, or the whole hospital) and `std::string adcName(AdcId)` giving `"ADC-<n>"`, or `"Hospital"` for 0 | all but 1, 2 |

Supporting types:

- `struct HistoryEntry { std::string kind; std::string who; std::string detail; long long time; AdcId adc; std::string medication; };`
  `adc` 0 means none; an empty `medication` means none.
- `enum class HistoryResult { Stored, StorageFailure };`
- `enum class ReadVerdict { Ok, StorageFailure };` and `struct HistoryReadResult { ReadVerdict verdict; std::vector<HistoryEntry> entries; };`
- `enum class CountVerdict { Ok, Unavailable };` and `struct HeartbeatCountResult { CountVerdict verdict; int count; };`
- `enum class SendResult { Sent, SendFailure };`

`MaintenanceStatus` (`virtual bool isInMaintenance(const AdcId& adc, long long now) = 0;`)
also sits in `interfaces/`, but it is **owned by this group** and implemented
by Maintenance. ADC heartbeats depends on the interface, not on Maintenance.

**Agreed permission table** (implemented by the fake; the real one belongs to
user settings):

| Action | Pharmacist | Technician | Administrator |
|--------|:---:|:---:|:---:|
| ScheduleMaintenance | | yes | |
| CancelMaintenance | | yes | |
| ManualInventoryChange | yes | | yes |
| AcknowledgeAlarm | yes | yes | yes |
| ResolveAlarm | yes | yes | yes |
| GenerateSummary | yes | | yes |
