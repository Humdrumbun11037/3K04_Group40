# ASSUMPTIONS_AND_DECISIONS: choices the code makes that D1 or the specs do not state

What this is: every decision or assumption in the code that D1 and the specs leave open, every place where code and spec disagree, and the decision lists from the team's notes. It ends with evidence for safety, privacy, access, accountability and accessibility.
How to read it: the tables give the file and line, the value chosen, the recorded reasoning, whether it matches the spec, the owner, and the status.
"Section 2" (D1) was not available. The "Matches spec?" column compares against Andrew's brief `HMS_behaviour_modules_brief.md` ("brief"), which quotes D1 values in places. Elsewhere it says "unknown".
Tags: [CODE] file:line read on 2026-11-06; [SPEC] the brief; [INFERRED] my reading. Status: **agreed** = a decision Andrew or the owner recorded; **to confirm** = still needs another teammate.
Paths: B/ = `Subgroup_B/behaviour_hiding/`, sw/ = `sw_decisions/`, hw/ = `Subgroup_B/hardware_hiding/`. Owners: A = Andrew, I = Inodh, M = Matthew, T = team.

## 1. Deliverable 1's original assumptions (as quoted in the brief)

The full D1 assumption list was not available. These are the ones the brief quotes.

| # | Assumption | Source | Where the code relies on it |
|---|------------|--------|-----------------------------|
| D1 | ADCs dispense independently of the HMS. | [SPEC brief §4.4 NAT] | B4a never sends to an ADC [CODE B/adc_heartbeats.hpp invariant] |
| D2 | Reporting periods are defined (heartbeat every 60 s; offline after 180 s). | [SPEC brief §3, §4.4] | B/adc_heartbeats.cpp:8-9 |
| D3 | Maintenance schedules are available. | [SPEC brief §4.4 NAT] | B4b `isInMaintenance` |
| D4 | Low-stock threshold 25%. | [SPEC brief §3] | B/low_stock_rule.cpp:6 |
| D5 | Daily report at 06:00 covering the previous 24 h. | [SPEC brief §3] | B/report_types.cpp:12-13 |
| D6 | Active history window 13 months. | [SPEC brief §3] | hw/long_term_storage.cpp `thirteenMonthsAgo`; sw/startup/daily_tasks.cpp:24-29 |
| D7 | Three roles: Pharmacist, Technician, Administrator. | [SPEC brief §3] | B/interfaces/user.hpp:13; sw/user_information/user_settings.hpp |

## 2. Behaviour hiding (Andrew)

| # | Decision | File:line | Value chosen | Reasoning recorded | Matches spec? | Owner | Status |
|---|----------|-----------|--------------|--------------------|---------------|-------|--------|
| A1 | Time representation | B/interfaces/clock.hpp; B/report_types.cpp:11-13 | `long long` seconds, hospital local time, no time zones | Subtraction is simple [CODE INTEGRATION_PROPOSAL §2] | unknown; Inodh and Matthew use text times | A | to confirm (M, I) |
| A2 | ADC id and "none" | B/interfaces/adc_id.hpp:15-21 | positive `int`; 0 = none, any, or whole hospital ("Hospital") | 0 is never a real ADC [CODE MODULE_NOTES notation] | unknown; Inodh uses -1 | A | to confirm (I) |
| A3 | No exceptions | all B files | every rejection is an `enum class` verdict; nothing throws | [SPEC brief §1a, §5] | yes (brief) | A | to confirm team-wide (I, M throw) |
| A4 | Exactly 25% | B/low_stock_rule.cpp:14 | not low (`count*100 < capacity*25`) | whole numbers avoid rounding | brief §4.2 leaves it open | A | agreed (A); to confirm (I) |
| A5 | Count of 0 | B/low_stock_rule.cpp:14; B/inventory_record.cpp:85 | valid, and low | | brief §4.5 leaves it open | A | agreed (A) |
| A6 | Scope of low stock | B/inventory_record.cpp `raiseIfLow`; B/hospital_stock.cpp | both per ADC (`InventoryLow`) and hospital-wide (`HospitalLowStock`) | D1 says all ADCs, Inodh says per ADC [SPEC brief §3] | covers both | A | agreed (A) |
| A7 | Hospital totals | B/hospital_stock.cpp:55-66 | reserve + every ADC, counts and capacities summed; reserves are one store | | not in spec (new module) | A | to confirm (T) |
| A8 | Hospital alarm timing | B/hospital_stock.cpp:87-90 | raised only on a not-low to low change, seen by `checkAll` | no repeat alarms while low | not in spec | A | agreed (A) |
| A9 | Reserve change permission | B/hospital_stock.cpp:33-34 | reuses `ManualInventoryChange` | same kind of act [CODE MODULE_NOTES §9] | not in spec | A | to confirm (I) |
| A10 | Deliveries and transfers | B/hospital_stock.* | not built; ADC deposits do not take from reserves | to agree with team first | not in spec | A | to confirm (T) |
| A11 | Offline alarm type | B/adc_heartbeats.cpp:68 | `AdcOffline` (Critical), not `ConnectionLoss` (Low) | TODO(Inodh) | brief §7 says ask | A | to confirm (I) |
| A12 | Start of monitoring | B/adc_heartbeats.cpp:15 | an ADC counts as heard from when monitoring starts | | not in spec | A | agreed (A) |
| A13 | Health history failure | B/adc_heartbeats.cpp:76 | ignored; never blocks a status change or alarm | safety first | not in spec | A | agreed (A) |
| A14 | Window shape | B/maintenance.cpp:22, 114 | half-open `[start, end)`; back-to-back allowed; may start in the past if it ends in the future | Andrew's decision | brief §4.3 silent | A | agreed (A); conflicts with I (S1) |
| A15 | Window length | B/maintenance.cpp:8, 23 | at most 12 h; exactly 12 h allowed | | not in brief; same as Inodh | A | agreed (A, I) |
| A16 | Who schedules or cancels maintenance | B/tests/fakes/fake_permissions.hpp:31-32 | Technician only | [SPEC brief §4.3 "Role is not Technician"] | yes (brief); Inodh has Administrator only | A | to confirm (I) |
| A17 | Cancelling | B/maintenance.cpp:36 | Scheduled or Active windows; Active ends suppression at once | Andrew's decision | not in spec | A | agreed (A) |
| A18 | Maintenance history failure | B/maintenance.cpp:91-95 | ignored; never blocks a change | | not in spec | A | agreed (A) |
| A19 | Escalation limits | B/alarm_resolution.cpp:12-13, 66 | Critical 120 s, High 600 s, Low never; fires at exactly the limit | [SPEC brief §7: D1 NFR-PRF-04] | yes (D1); Inodh uses 6 min | A | agreed (A); to confirm (I) |
| A20 | Escalation form | B/alarm_resolution.cpp:142-155 | changes the existing alarm (reroute or raise severity) | TODO(Inodh) | **no**: brief §4.6 says "new alarm" | A | to confirm (I) |
| A21 | Escalation if history fails | B/alarm_resolution.cpp:153 | still escalates | reaching a supervisor matters more than the record | not in spec | A | to confirm (T) |
| A22 | Supervisor chain | B/alarm_resolution.cpp:26-30 | Technician and Pharmacist to Administrator; Administrator none | Andrew's brief answers | not in D1 | A | agreed (A) |
| A23 | Resolve without acknowledging | B/alarm_resolution.cpp:55 (T3) | allowed; counts as an implicit acknowledgement | Andrew's decision | not in spec | A | agreed (A) |
| A24 | Invalid alarm action (for example acknowledging a resolved alarm) | B/alarm_resolution.cpp:118, 133 | returns a verdict (`NotRaised`, `AlreadyResolved`), changes nothing, **writes no log entry** | logging every rejected action would flood the audit trail | **no**: departs from the Week 6 slide ("no change plus a log entry"; brief §4.6). Easy to change: one `store(...)` call per rejecting row. | A | agreed (A) |
| A25 | Duplicate alarms | B/alarm_resolution.cpp:100 | join only if ADC, type **and detail** match | keeps different medications apart | partly: FR-ALM-04 says type and ADC [SPEC brief §7] | A | agreed (A) |
| A26 | Acknowledge / resolve permission | fake_permissions.hpp:33-34 | all three roles, asked separately | | not in spec | A | to confirm (I) |
| A27 | Unknown alarm type | B/alarm_severity.cpp:19 | `UnknownType`, with Critical as filler | errs on the safe side | yes ("never guessed") | A | agreed (A) |
| A28 | Destination role | demo_main.cpp:89-97 | demo guess: stock alarms to Pharmacist, unauthorized to Administrator, rest to Technician | destination is notification's job | not in spec; Inodh routes by severity | I | to confirm (I) |
| A29 | Over-withdrawal | B/inventory_record.cpp:50-55 | recorded, count 0, `InventoryDiscrepancy` (High), verdict `DiscrepancyRecorded` | the ADC already dispensed; matches Inodh's router | not in brief | A | agreed (A); High to confirm (I) |
| A30 | Zero amounts | B/inventory_record.cpp:42, 66 | withdraw or deposit of 0 rejected (`IncorrectValue`) | | not in spec; Inodh allows 0 | A | agreed (A); to confirm (I) |
| A31 | Above capacity | B/inventory_record.cpp:66, 85 | allowed for deposits and manual changes; overflow guarded | | not in spec; Inodh caps at 1,000,000 | A | agreed (A) |
| A32 | Count changes only if recorded | B/inventory_record.cpp:48, 70, 89 | history failure means `StorageFailure`, nothing changes | | yes (brief: "change recorded") | A | agreed (A) |
| A33 | `addItem` | B/inventory_record.cpp:28 | rejects adc <= 0; writes no history; raises no alarm | set-up only | not in spec | A | agreed (A) |
| A34 | Daily report timing | B/report_types.cpp:11-13, 76-87 | day = floor(s/86400); due at or after 06:00; window `[now-24h, now)`; marked done only after a successful send; retried each tick | [SPEC brief §4.7] | yes | A | agreed (A) |
| A35 | "Needs attention" | B/report_types.cpp:16, 108 | fewer heartbeats than one per 60 s; decommissioned not flagged; in maintenance still flagged | | brief: "fewer than expected" | A | agreed (A) |
| A36 | Report sections | B/report_types.cpp:22-30 | by start of history kind: `Inventory…`/`ReserveChange`, `Alarm…`, `AdcHealth`/`Maintenance`, else Events | | not in spec | A | agreed (A) |
| A37 | Summary window | B/report_types.cpp:124-140 | `[from, to]` both ends included; daily is `[from, to)` | | not in spec | A | agreed (A) |
| A38 | Report failures logged | B/report_types.cpp:119 | `ReportFailed` entry in history | | yes ("failure logged") | A | agreed (A) |
| A39 | Date display | B/report_formatting.cpp:21-31 | `YYYY-MM-DD HH:MM:SS` by hand, no time-zone library; ADC 0 shown as "Hospital" | | not in spec | A | agreed (A) |
| A40 | History must report failure | B/interfaces/history.hpp | `record()` returns `Stored` or `StorageFailure` | needed for "resolved only if recorded" | Inodh's `record` returns `void` | A | to confirm (I) |
| A41 | Role rules | B/interfaces/permissions.hpp | modules ask `Permissions`; the one exception is the supervisor chain (A22) | change one table, not modules | not in spec | A | agreed (A) |

## 3. Software decisions (Inodh)

The first nine rows are the list in `PROGRESS.md` (repo root) "Decisions made that the group should confirm". The rest are read from the code.

| # | Decision | File:line | Value chosen | Matches spec? | Owner | Status |
|---|----------|-----------|--------------|---------------|-------|--------|
| I1 | ADC message format | sw/events_logs/event_formatting.h | `<adcId>|<keyword>|<fields>`; patient fields start `PT_` | unknown | I | to confirm (T) |
| I2 | Timestamps | sw/external_interfaces.hpp `Clock` | text `YYYY-MM-DD HH:MM:SS` everywhere | unknown; conflicts with A1 | I | to confirm (T) |
| I3 | Role permissions | sw/user_information/user_settings.cpp:16-53 | manual inventory: Pharmacist only; maintenance: Administrator only; no acknowledge permission | conflicts with A16, A26 and the proposal table | I | to confirm (I, A) |
| I4 | Unregistered ADCs | sw/events_logs/message_routing.cpp:80 | messages dropped (`UnknownAdc`) | unknown | I | to confirm (T) |
| I5 | Over-withdrawal | message_routing.cpp:130-138 | count 0, `DISCREPANCY` alarm | now matches A29 | I | agreed (A, I) |
| I6 | No one in a role | sw/alarm_notification.cpp:100-103 | alarm goes to administrators | unknown | I | to confirm (T) |
| I7 | Escalation | sw/alarm_notification.hpp `kEscalationSeconds`; .cpp:61-87 | 6 min, to administrators, once | **no**: D1 NFR-PRF-04 is 2/10 min [SPEC brief §7] | I | to confirm (I) |
| I8 | Maintenance windows | sw/maintenance_scheduler.cpp:133-145 | at most 12 h, no overlap, start not in the past, -1 = all ADCs | conflicts with A14 | I | to confirm (I, A) |
| I9 | Sessions | sw/startup/authorization.hpp `kSessionTimeoutSeconds` | end after 15 min idle | unknown | I | to confirm (T) |
| I10 | Password hashing | sw/user_information/user_registry.cpp:133-150 | salted, 10,000-round FNV-1a, marked placeholder | unknown | I | to confirm (T) |
| I11 | Lockout | user_registry.hpp `kMaxFailedAttempts`; .cpp:59-62 | locked after 4 consecutive failures; unlocked by `unlockAccount` | code cites FR-USR-03 | I | agreed (I) |
| I12 | Login message | sw/startup/authorization.cpp:33-36 | same message for unknown id and wrong password | unknown | I | agreed (I) |
| I13 | Recipients | alarm_notification.cpp:29-36 | Low: Technician; High: Pharmacist; Critical: Administrator and Technician | unknown | I | to confirm (I) |
| I14 | Deposit of unknown medication | message_routing.cpp:118-124 | accepted: count starts at 0 and a new item is created | conflicts with B3a (`UnknownItem`) | I | to confirm (I, A) |
| I15 | Quantity 0 | sw/events_logs/event_contents.cpp:17-31 | allowed ("non-negative") | conflicts with A30 | I | to confirm (I) |
| I16 | Manual change rules | sw/manual_inventory_changes.cpp:37-66 | +/- deltas; zero change rejected; result over 1,000,000 rejected; all or nothing; **no explanation** | **no**: FR-INV-04 needs an explanation [SPEC brief §4.5] | I | to confirm (I) |
| I17 | Archiving | sw/startup/daily_tasks.cpp:24-29 | on the first tick of each calendar day | brief: 13 months | I | agreed (I) |
| I18 | Encryption | sw/message_processing/encryption.hpp | ChaCha20-Poly1305, 32-byte key, `[nonce][ciphertext][tag]` | unknown | I | agreed (I) |
| I19 | Key file | sw/startup/encryption_key_storage.cpp | raw 32-byte file, created if missing | unknown | I | to confirm (M notes OS protection) |
| I20 | Startup checks | sw/startup/initialize.cpp:17-34 | throw `std::logic_error` if a role has no permissions or admins can't manage users | unknown | I | agreed (I) |
| I21 | First administrator | initialize.cpp:53-56 | only while there is none | unknown | I | agreed (I) |
| I22 | Escalated alarms | alarm_notification.cpp:85 | removed from `pending()` once escalated, even if unresolved | risk to NFR-SAF-02 [INFERRED] | I | to confirm (I) |
| I23 | Finished windows | sw/maintenance_scheduler.cpp:89-95 | deleted at `tick()` | unknown | I | agreed (I) |
| I24 | Exceptions | encryption.cpp; user_registry.cpp:106; report_generation.cpp:59 | thrown for a bad key, a tampered packet, an unknown user, a failed report | conflicts with A3 | I | to confirm (T) |

## 4. Hardware hiding (Matthew)

| # | Decision | File | Value chosen | Matches spec? | Owner | Status |
|---|----------|------|--------------|---------------|-------|--------|
| M1 | Clock | hw/clock/clock.cpp | local time via `std::localtime`, as text | conflicts with A1 | M | to confirm (T) |
| M2 | Storage errors | hw/long_term_storage.cpp | exceptions | conflicts with A3; "storage failure must not be treated as success" [CODE hw/MODULE_NOTES §5] | M | to confirm (T) |
| M3 | Search range | hw/long_term_storage.cpp `search` | `from <= t <= to` | B's `HistoryReader` reads `[from, to)` | M | to confirm (adapter) |
| M4 | Retention | long_term_storage.cpp `thirteenMonthsAgo` | 13 calendar months | yes (brief §3) | M | agreed |
| M5 | Comms link packets | hw/communications_link/communications_link.cpp | raw string bytes; empty string = none | the router expects encrypted packets | M | to confirm (M, I) |
| M6 | Dashboard | hw/dashboard_manager/dashboard_manager.cpp | prints to the console | unknown | M | to confirm (T) |

## 5. Places where code and spec (or two codebases) disagree

| # | Topic | One side | Other side | Proposed |
|---|-------|----------|------------|----------|
| X1 | Escalation time | B1b 2/10 min (D1) | S7 6 min | keep B1b [CODE INTEGRATION_PROPOSAL §1] |
| X2 | Escalation form | brief §4.6: new alarm | B1b changes the existing alarm | open (Inodh) |
| X3 | Invalid alarm action | Week 6 slide / brief §4.6: log entry | B1b: no log entry | decided: keep B1b (A24); a deliberate departure, easy to change |
| X4 | Duplicate alarm match | FR-ALM-04: ADC + type | B1b: ADC + type + detail | keep detail; tell team |
| X5 | Maintenance permission | brief §4.3, B4b: Technician | S8a: Administrator | proposal: Technician |
| X6 | Maintenance in the past | B4b allows a window under way | S1 rejects a past start | open |
| X7 | "All ADCs" windows | S1 has them | B4b does not | open |
| X8 | Manual change explanation | FR-INV-04, B3a: required | S9: not asked for | add to S9 |
| X9 | Time type | B: seconds | S, H: text | proposal: seconds |
| X10 | "No ADC" | B: 0 | S: -1 | open |
| X11 | Offline alarm type | FR-COM-04: one communication-loss alarm | B4a: `AdcOffline`; table also has `ConnectionLoss` | open (Inodh) |
| X12 | Name clashes | B: classes `AlarmSeverity`, `ReportTypes` | S: enum `AlarmSeverity`, interface `ReportTypes` | namespaces (proposal §2) |
| X13 | Deposit of unknown item | B3a: `UnknownItem` | S6c: creates the item | open |
| X14 | Docs out of date | INTEGRATION_PROPOSAL "Where things stand" said the makefile builds only behaviour hiding | the makefile also builds hardware hiding [CODE B/makefile] | fixed 2026-11-06 |
| X15 | Docs out of date | PROGRESS.md "Next: hw_hiding" | hw_hiding now exists | Inodh to update |

## 6. Accessibility and ethics evidence

| Area | Code evidence | Gap |
|------|---------------|-----|
| **Patient safety** | B4a raises an alarm after 3 missed periods and never sends to an ADC (NFR-SAF-01) [CODE B/adc_heartbeats.*]. B1b escalates unacknowledged alarms (2/10 min) and keeps every alarm until resolved [CODE B/alarm_resolution.cpp]. B3a/B3c raise low-stock and discrepancy alarms; an over-withdrawal is recorded, not refused, because the ADC already dispensed [CODE B/inventory_record.cpp:50]. History failures never block an alarm or escalation (A13, A21). Unknown alarm types default to Critical (A27). | Behaviour and software decisions are not connected, so in a real run no B alarm reaches a person yet. S7 drops escalated alarms from `pending()` (I22). Exceptions in S/H could stop the main loop (I24, M2). |
| **Privacy of patient data** | S5b strips every `PT_` field before anything else sees the message, ignoring case and leading spaces, and wipes its copy [CODE sw/message_processing/patient_info_stripping.cpp:37-71]. S6c strips inside `routePacket` [CODE message_routing.cpp:70-71]. Storage is encrypted at rest [CODE hw/long_term_storage.cpp]. No B module stores patient data: history entries hold user name, ADC and medication only [CODE B/interfaces/history.hpp]. | Patient fields are recognised only by the `PT_` prefix (I1); a field without it would pass through. The key file is unprotected raw bytes (I19). No tests for stripping in the repo. |
| **Controlled access: login** | `Authorization::login`, sessions ending after 15 min idle, `authorize` on every action [CODE sw/startup/authorization.cpp]. | B modules take a `User` as given; nothing in B checks the user is logged in (it relies on S4a). |
| **Controlled access: lockout** | Locks after 4 consecutive failures; only `unlockAccount` reopens it; the same message for an unknown id and a wrong password [CODE sw/user_information/user_registry.cpp:59-62; authorization.cpp:33-36]. Startup refuses a table where administrators can't unlock [CODE initialize.cpp:29-32]. | Placeholder password hash (I10). No test in the repo. |
| **Controlled access: permissions** | Every B action asks `Permissions::mayPerform`; each has a `row1` test and a `follows_the_permission_table` test [CODE B/tests]. S8a holds Inodh's table. | The two tables disagree (I3, X5). B runs only on the fake table. |
| **Accountability: audit entries** | B writes a history entry for every alarm acknowledgement, resolution and escalation, inventory change (withdraw, deposit, discrepancy, manual, reserve), maintenance change, ADC health change, report failure and summary request, with who, when, ADC and medication [CODE B/*.cpp `history.record`]. A count or alarm state changes only if its entry was stored (A32). Every summary request, even a refused one, writes exactly one audit entry [CODE B/report_types.cpp:154]. S logs logins, registry changes, maintenance and manual changes [CODE sw]. | B's entries go only to the fake history. `record()` on Inodh's side returns `void`, so failures can't be seen (A40). |
| **Accountability: required notes** | Resolving an alarm needs a non-blank note (`NoExplanation`) [CODE B/alarm_resolution.cpp:134]. Manual ADC and reserve changes need a non-blank explanation [CODE B/inventory_record.cpp:86; B/hospital_stock.cpp:38]. | S9's manual change takes no explanation (I16). |
| **Accessibility** | Severity is given as a word as well as a level (`Low/High/Critical`) in the demo and reports [CODE demo_main.cpp `SEVERITY`; sw alarm_notification.cpp `severityName`]. Reports are plain text with fixed sections and "(none)" for empty ones [CODE B/report_formatting.cpp]. Error replies are plain sentences (`describe` functions in S3, S4a, S1). | No GUI exists yet, so nothing on contrast, font size, keyboard use or screen readers. No colour is used, so nothing depends on colour alone. NFR-USE-01..03 are not tested. |
