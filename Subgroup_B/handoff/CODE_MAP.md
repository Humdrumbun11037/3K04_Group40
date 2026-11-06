# CODE_MAP: every module in the HMS module tree and the code behind it

What this is: a map from each module in the team's HMS module tree to its owner, files, classes, public functions, tests and status, as of 2026-11-06.
How to read it: one section per branch (hardware hiding, software decisions, behaviour hiding); each module is a row, and its public functions follow in a table.
Module IDs (H1, S6c, B3a, ...) were made up for these handoff files and follow the order of the team tree; the same IDs are used in every handoff file.
Tags: [CODE] = read in the named file and function; [SPEC] = stated in Andrew's brief `HMS_behaviour_modules_brief.md` (his summary of D1 and teammates' specs, kept outside the repo); [INFERRED] = my reading, not stated anywhere.
Paths are relative to the repo root `3K04_Group40/`. "B/" means `Subgroup_B/`, "sw/" means `sw_decisions/`, "hw/" means `Subgroup_B/hardware_hiding/`.

## Status words

| Word | Meaning |
|------|---------|
| built | Code exists, compiles, and does the module's whole job as far as the code shows. |
| partial | Code exists and compiles, but part of the job is missing or is a placeholder. |
| stub | Code exists but only prints or returns fixed values. |
| missing | No code. |

"Tested" means there is a test program in the repo. Only Andrew's behaviour-hiding modules have tests in the repo [CODE `B/tests/`]. Matthew's notes say his encrypted-storage test passed, but that test is not in the repo [CODE `hw/MODULE_NOTES.md` "Verified implementation and testing status"].

## Build and run (from `Subgroup_B/`, MinGW on Windows)

| Command | What it does |
|---------|--------------|
| `mingw32-make` (target `all`) | Compiles every `.cpp` in `behaviour_hiding/` and `hardware_hiding/`, plus `../Subgroup_A/src/HW1_Transceiver.cpp`, into `obj/`. [CODE `B/makefile`] |
| `mingw32-make test` | Builds one program per `tests/*.cpp`, links it with the behaviour-hiding objects only, and runs them all; stops at the first failure. |
| `mingw32-make demo` | Builds and runs `bin/demo` (the console demo in `demo_main.cpp`, behaviour hiding plus fakes). |
| `mingw32-make clean` | Deletes `obj/` and `bin/`. |

`sw_decisions/` is not built by any makefile. I checked each of its 15 `.cpp` files with `g++ -std=c++17 -Wall -Wextra -fsyntax-only -I.` on 2026-11-06: all 15 compile with no warnings.

**Test counts I ran (2026-11-06, `mingw32-make test`): 215 tests, all pass.**

| Test program | Tests |
|--------------|------:|
| `adc_heartbeats_test` | 19 |
| `alarm_resolution_test` | 39 |
| `alarm_severity_test` | 8 |
| `hospital_stock_test` | 30 |
| `inventory_record_test` | 36 |
| `low_stock_rule_test` | 9 |
| `maintenance_test` | 38 |
| `report_formatting_test` | 10 |
| `report_types_test` | 26 |

## Summary of the tree

| ID | Module | Owner | Code | Status |
|----|--------|-------|------|--------|
| H1 | Long-term storage | Matthew | `hw/long_term_storage.*` `FileAuditTrail` | built, no test in repo |
| H1a | Audit trail | Matthew | `FileAuditTrail::append`, `archiveOlderThan13Months` | built, no test in repo |
| H1b | Records searching | Matthew | `FileAuditTrail::search` | built, no test in repo |
| H2 | Clock | Matthew | `hw/clock/clock.*` `SystemClock` | built (gives text time; behaviour hiding wants seconds) |
| H3 | Dashboard manager | Matthew | `hw/dashboard_manager/*` `ConsoleDashboardManager` | partial (prints messages to the console) |
| H3a | Dashboard content | Matthew | none separate | missing (folded into H3) |
| H3b | Dashboard formatting | Matthew | none separate | missing (folded into H3) |
| H3c | User inputs | Matthew | `hw/use_Input_Handler/*` `ConsoleInputHandler` | built (reads one console line) |
| H4 | Comms link | Matthew | `hw/communications_link/*` `HardwareCommsLink` | partial (wraps Subgroup A's stub transceiver) |
| S1 | Maintenance scheduler | Inodh | `sw/maintenance_scheduler.*` | built, overlaps B4b |
| S2 | Report generation | Inodh | `sw/report_generation.*` | built, overlaps B2 |
| S3 | ADC registry | Inodh | `sw/adc_registry.*` | built |
| S4 | Startup | Inodh | `sw/startup/` | built |
| S4a | Authorization | Inodh | `sw/startup/authorization.*` | built |
| S4b | Initialize | Inodh | `sw/startup/initialize.*` `HMSystem` | built (not yet wired to behaviour hiding) |
| S4c | Daily tasks | Inodh | `sw/startup/daily_tasks.*` | built |
| S5 | Message processing | Inodh | `sw/message_processing/` | built |
| S5a | Encryption | Inodh | `sw/message_processing/encryption.*` (+ `sw/startup/encryption_key_storage.*`) | built |
| S5b | Patient info stripping | Inodh | `sw/message_processing/patient_info_stripping.*` | built |
| S6 | Event logs | Inodh | `sw/events_logs/` | built |
| S6a | Event contents | Inodh | `sw/events_logs/event_contents.*` | built |
| S6b | Events formatting | Inodh | `sw/events_logs/event_formatting.h` | built |
| S6c | Message routing | Inodh | `sw/events_logs/message_routing.*` | built |
| S7 | Alarm notification | Inodh | `sw/alarm_notification.*` | built, overlaps B1b on escalation |
| S8 | User information | Inodh | `sw/user_information/` | built |
| S8a | User settings | Inodh | `sw/user_information/user_settings.*` | built |
| S8b | User registry | Inodh | `sw/user_information/user_registry.*` | built (password hash is a placeholder) |
| S9 | Manual inventory changes | Inodh | `sw/manual_inventory_changes.*` | built, overlaps B3a |
| B1a | Alarm severity | Andrew | `B/behaviour_hiding/alarm_severity.*` | built, tested (8) |
| B1b | Alarm resolution | Andrew | `B/behaviour_hiding/alarm_resolution.*` | built, tested (39) |
| B2a | Report types | Andrew | `B/behaviour_hiding/report_types.*` | built, tested (26) |
| B2b | Report formatting | Andrew | `B/behaviour_hiding/report_formatting.*` | built, tested (10) |
| B3a | Inventory record | Andrew | `B/behaviour_hiding/inventory_record.*` | built, tested (36) |
| B3b | Low stock rule | Andrew | `B/behaviour_hiding/low_stock_rule.*` | built, tested (9) |
| B3c | Hospital inventory | Andrew | `B/behaviour_hiding/hospital_stock.*` `HospitalStock` | built, tested (30); deliveries and transfers not built |
| B4a | ADC heartbeats | Andrew | `B/behaviour_hiding/adc_heartbeats.*` | built, tested (19) |
| B4b | Maintenance | Andrew | `B/behaviour_hiding/maintenance.*` | built, tested (38) |

Not in the tree, but in the code:
- `sw/external_interfaces.hpp`: the interfaces Inodh's code needs from the other branches.
- `sw/startup/encryption_key_storage.*`: loads or creates the key file.
- `B/behaviour_hiding/interfaces/`: the interfaces Andrew's code needs, in namespace `hms::behaviour`.
- `B/demo_main.cpp` and `B/tests/` with `B/tests/fakes/`.
- `Subgroup_A/` is the ADC team's program. The HMS uses only its `HW1_Transceiver` [CODE `B/makefile`].

---

## Hardware hiding (Matthew)

All five classes implement Inodh's interfaces in `sw/external_interfaces.hpp`, not Andrew's `hms::behaviour` ones [CODE `hw/*/*.hpp` includes].

| Class | Function | What it does |
|-------|----------|--------------|
| `FileAuditTrail` (H1) | `FileAuditTrail(activeFilename, longTermFilename, EncryptionManager&)` | Opens (creating folders if needed) the active and archive files, which are encrypted. |
| | `void append(const Event&)` | Encrypts one event and adds it to the active file with a sequence number. Throws on a bad timestamp or a file error. |
| | `std::vector<Event> search(from, to) const` | Returns every stored event (active plus archive) with from <= time <= to, oldest first. Throws on a bad range or a damaged record. |
| | `void archiveOlderThan13Months(now)` | Moves records more than 13 calendar months old into the archive file, then rewrites the active file. |
| `SystemClock` (H2) | `std::string now() const` | Local computer time as `YYYY-MM-DD HH:MM:SS`. |
| `ConsoleDashboardManager` (H3) | `void deliver(userId, message)` | Prints the message under a "Dashboard for user" banner. |
| `ConsoleInputHandler` (H3c) | `std::optional<UserInput> getInput()` | Prints `Enter: ` and reads one line; nothing at end of input. |
| `HardwareCommsLink` (H4) | `std::optional<std::vector<uint8_t>> receive()` | Calls `HW1::Receive_Message()`; an empty string means no packet, anything else becomes the packet bytes. |

[CODE `Subgroup_A/src/HW1_Transceiver.cpp`] `Receive_Message()` always returns `"HMS_ACK"`, so `HardwareCommsLink::receive()` never reports "no packet". [INFERRED] If it were plugged into `MessageRouter::pollLink`, which loops until `receive()` is empty, that loop would never end. See OPEN_QUESTIONS.

---

## Software decisions (Inodh)

None of these has a test in the repo. `PROGRESS.md` (repo root) lists all 14 tree modules as "done".

| Module | Class | Function | What it does |
|--------|-------|----------|--------------|
| S1 | `MaintenanceScheduler` | `scheduleMaintenance(start, end, adcId = kAllAdcs, scheduledBy, newId*)` | Checks and books a window. Times are text, at most 12 h, not in the past, no overlap; `-1` means all ADCs. |
| | | `bool cancel(windowId)` | Removes a window that has not started. |
| | | `void tick()` | Starts and finishes windows by the clock; finished ones are deleted. |
| | | `maintenanceActive()` / `maintenanceActive(adcId)` | Whether any window, or one covering this ADC, is active now. |
| | | `upcoming()`, `describe(Result)` | Lists pending and active windows; gives a message for a result. |
| S2 | `ReportGenerator` (also an `EventRecorder`) | `void record(const Event&)` | Stores one event in the audit trail, filling in the time if blank. |
| | | `Result generate(type, from, to) const` | Builds a report of a named type over [from, to] using the `ReportTypes` interface. |
| | | `generateEncrypted(type, from, to, EncryptionManager&)` | Same, then encrypts it. Throws if the report fails. |
| S3 | `ADClist` | `addADC(id, location, byUser)`, `removeADC(id, byUser)`, `relocateADC(id, newLocation, byUser)` | Register, remove and move ADCs; each change is logged. |
| | | `exists(id)`, `find(id)`, `all()`, `describe(Result)` | Look-ups and messages. |
| S4a | `Authorization` | `LoginResult login(userId, password)` | Checks the password, reports a locked account, starts a session, logs the attempt. |
| | | `logout(userId)`, `session(userId)`, `isLoggedIn(userId)` | Session handling; a session ends after 15 min idle. |
| | | `bool authorize(userId, Permission)` | True if logged in and the role allows the action; counts as activity. |
| S4b | `HMSystem` | `HMSystem(ExternalModules, EncryptionManager&)` | Builds and connects every sw_decisions module; throws if the role table is unusable. |
| | | `createInitialAdministrator(userId, password)` | Makes the first administrator, only while there is none. |
| | | `void tick()` | One main-loop pass: route messages, escalate alarms, maintenance tick, daily tasks. |
| S4c | `DailyTasks` | `void tick()` | Offers the daily report a chance to run on every tick; archives once per calendar day. |
| S5a | `EncryptionManager` | `decryptADCMessage`, `encryptForADC`, `encryptForReport`, `encryptData`, `decryptData` | Encrypts and decrypts with ChaCha20-Poly1305 (AES-256-GCM if built with OpenSSL). Throws on a bad key or a tampered packet. |
| | | `defaultStrategy()`, `generateKey()` | Picks the algorithm; makes a random 32-byte key. |
| | (free function) | `loadOrCreateEncryptionKey(filename)` | Reads the key file, or creates one. Throws on a bad file. |
| S5b | `PatientInfoStripper` | `std::string strip(message) const` | Drops every `|`-separated field starting `PT_` (any case, leading spaces ignored). |
| | | `bool isPatientField(field) const` | Whether one field is patient data. |
| S6a | `EventScanner` | `std::optional<Event> scan(adcId, tokens)` | Finds an event keyword, reads its fields, validates and logs it. |
| S6b | (header only) | `eventRules()`, `findRule(keyword)`, `eventTypeName(type)`, `eventTypeFromName(name, out)` | The keyword table: Alarm, Withdrawal, Deposit, Login and their fields. |
| S6c | `MessageRouter` | `Outcome routePacket(packet)` | Decrypt, strip patient data, then route. |
| | | `Outcome routeMessage(stripped)` | Drops unknown ADCs; heartbeats go to `HeartbeatMonitor`, alarms to `AlarmNotifier`, withdrawals and deposits to `InventoryStore`. |
| | | `std::size_t pollLink(CommsLink&)` | Routes every waiting packet. |
| S7 | `AlarmNotifier` (Inodh's, not Andrew's interface of the same name) | `int raise(adcId, code, description)` | Classifies the code, tracks it, and sends it to the roles for its severity. |
| | | `std::vector<int> checkUnresolved()` | Re-sends alarms unresolved for 6 min to administrators, once. |
| | | `pending()`, `recipientsFor(severity)`, `severityName(severity)` | Open alarms; Low to Technician, High to Pharmacist, Critical to Administrator and Technician. |
| S8a | `UserSettings` | `isAllowed(role, permission)`, `settingsFor(role)`, `allRoles()`, `roleName`, `roleFromName`, `permissionName` | The role-to-permission table. |
| S8b | `UserRegistry` | `addUser`, `removeUser`, `authenticate`, `exists`, `isAccountLocked`, `unlockAccount`, `changePassword`, `getUserRole`, `usersWithRole` | Accounts with salted hashes; locks after 4 failures; `getUserRole` throws for an unknown user. |
| S9 | `ManualInventoryChanges` | `Result apply(userId, adcId, changes)` | Applies +/- changes to one ADC, all or nothing; rejects unknown ADC or medication, a zero change, a negative or over 1,000,000 result; tells the user. |

---

## Behaviour hiding (Andrew)

All modules: no input or output, no exceptions, rejections are enum verdicts [CODE headers; SPEC brief §1a]. Interfaces are in `B/behaviour_hiding/interfaces/`, namespace `hms::behaviour`, each with a fake in `B/tests/fakes/`.

| Module | Class | Function | What it does |
|--------|-------|----------|--------------|
| B1a | `AlarmSeverity` | `SeverityResult severityOf(AlarmType) const` | Looks up an alarm type's severity; unknown types are rejected. |
| B1b | `AlarmResolution` | `AddResult add(adc, type, destination, detail = "")` | Records a new alarm, or joins an open one with the same ADC, type and detail. |
| | | `AcknowledgeVerdict acknowledge(user, id)` | Acknowledges a Raised alarm if permitted and recorded; stops escalation. |
| | | `ResolveVerdict resolve(user, id, note)` | Resolves an alarm, but only with a note and only if the record is stored. |
| | | `std::vector<Escalation> escalateDue()` | Escalates unacknowledged alarms past their limit (Critical 2 min, High 10 min). |
| | | `std::optional<AlarmInfo> find(id) const` | A copy of one alarm's details. |
| B2a | `ReportTypes` | `DailyVerdict generateDaily(now)` | Sends one daily report per day at or after 06:00 for the last 24 h; retries on failure. |
| | | `SummaryResult summary(user, filters)` | Filtered activity summary; every request writes one audit entry. |
| B2b | `ReportFormatting` | `formatTime(seconds)`, `formatDaily(report)`, `formatSummary(summary)` | Plain-text layout, with dates worked out by hand. |
| B3a | `InventoryRecord` | `addItem(adc, med, count, capacity)` | Starts tracking a medication in an ADC. |
| | | `withdraw(adc, med, amount)` | Lowers the count; an over-withdrawal sets 0 and raises a discrepancy alarm. |
| | | `deposit(adc, med, amount)` | Raises the count (above capacity allowed). |
| | | `manualChange(user, adc, med, newCount, explanation)` | Authorized correction with a required explanation. |
| | | `count(adc, med)`, `items()` | Read the counts. |
| B3b | `LowStockRule` | `LowStockVerdict check(count, capacity) const` | Low if strictly below 25% of capacity. |
| B3c | `HospitalStock` | `addReserve(med, count, capacity)` | Starts tracking a pharmacy reserve. |
| | | `manualReserveChange(user, med, newCount, explanation)` | Authorized reserve correction with an explanation. |
| | | `check(med)`, `all()` | Hospital totals (reserve plus every ADC) and Low/NotLow. |
| | | `std::vector<std::string> checkAll()` | Raises `HospitalLowStock` when a medication turns low. |
| B4a | `AdcHeartbeats` | `monitor(adc)`, `stopMonitoring(adc)` | Start or stop watching an ADC. |
| | | `heartbeatReceived(adc)` | Marks the ADC heard from and Online. |
| | | `void checkAll()` | Marks silent ADCs Offline after 3 missed 60 s periods and raises `AdcOffline`, except in maintenance. |
| | | `AdcStatus status(adc) const` | Online, Offline or NotMonitored. |
| B4b | `Maintenance` | `schedule(user, adc, start, end)` | Books a window (Technician; at most 12 h; no overlap). |
| | | `cancel(user, adc, start)` | Cancels a Scheduled or Active window. |
| | | `bool isInMaintenance(adc, now)` | Whether the ADC has an Active window now. |

### Tests and demo per behaviour module

Test names follow the function-table rows (`row3_...`). Full names are listed in TRACEABILITY.md.

| Module | Test file | Demo commands (`demo_main.cpp`) |
|--------|-----------|-------------------------------|
| B1a | `tests/alarm_severity_test.cpp` | `alarm ADC TYPE`, `alarms` |
| B1b | `tests/alarm_resolution_test.cpp` | `alarms`, `ack N`, `resolve N NOTE`, `advance N` (escalation) |
| B2a | `tests/report_types_test.cpp` | `report`, `summary FROM TO [adc= med= actor=]` |
| B2b | `tests/report_formatting_test.cpp` | output of `report` and `summary` |
| B3a | `tests/inventory_record_test.cpp` | `stock`, `withdraw`, `deposit`, `set` |
| B3b | `tests/low_stock_rule_test.cpp` | used by `withdraw`, `set`, `hospital` |
| B3c | `tests/hospital_stock_test.cpp` | `hospital [MED]`, `reserve MED N EXPLANATION` |
| B4a | `tests/adc_heartbeats_test.cpp` | `heartbeat`, `silence`, `resume`, `status`, `scenario offline` |
| B4b | `tests/maintenance_test.cpp` | `schedule`, `cancel`, `scenario maintenance` |

The demo also has: `time`, `user ROLE`, `add-adc`, `decommission`, `history-fail` and `sender-fail` (make the fake history or report sender fail), and `quit` [CODE `demo_main.cpp` `help`, `run`]. In the demo the clock, history, ADC registry, heartbeat counter, report sender, permissions and alarm notifier are fakes. All nine behaviour modules are real.
