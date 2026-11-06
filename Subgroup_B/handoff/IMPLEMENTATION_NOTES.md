# IMPLEMENTATION_NOTES: how the Deliverable 1 design shows up in the code

What this is: notes for the design document. It covers modules to files and tests, D1 models to code, how each secret is hidden, and what is connected.
How to read it: four sections, each a table. Module IDs are the ones in CODE_MAP.md. B/ = `Subgroup_B/`, sw/ = `sw_decisions/`, hw/ = `Subgroup_B/hardware_hiding/`.
The D1 model documents were not available. Model names come from the task list, and what each model shows is [INFERRED] from its name.
Tags: [CODE] read in the named file; [SPEC] Andrew's brief `HMS_behaviour_modules_brief.md`; [INFERRED] my reading.
Status as of 2026-11-06: 215 tests pass (`mingw32-make test` in Subgroup_B).

## 1. Modules, files, classes and tests

| ID | Module | File(s) | Class | Tests |
|----|--------|---------|-------|-------|
| H1 | Long-term storage (audit trail, records searching) | hw/long_term_storage.* | `FileAuditTrail` | none in repo |
| H2 | Clock | hw/clock/clock.* | `SystemClock` | none |
| H3 | Dashboard manager | hw/dashboard_manager/* | `ConsoleDashboardManager` | none |
| H3c | User inputs | hw/use_Input_Handler/* | `ConsoleInputHandler` | none |
| H4 | Comms link | hw/communications_link/* | `HardwareCommsLink` | none |
| S1 | Maintenance scheduler | sw/maintenance_scheduler.* | `MaintenanceScheduler`, `MaintenanceWindow` | none |
| S2 | Report generation | sw/report_generation.* | `ReportGenerator`, `EventRecorder` | none |
| S3 | ADC registry | sw/adc_registry.* | `ADClist` | none |
| S4a/b/c | Authorization, Initialize, Daily tasks | sw/startup/* | `Authorization`, `HMSystem`, `DailyTasks` | none |
| S5a/b | Encryption, Patient info stripping | sw/message_processing/* | `EncryptionManager`, `PatientInfoStripper` | none |
| S6a/b/c | Event contents, Events formatting, Message routing | sw/events_logs/* | `EventScanner`, (rule table), `MessageRouter` | none |
| S7 | Alarm notification | sw/alarm_notification.* | `AlarmNotifier` | none |
| S8a/b | User settings, User registry | sw/user_information/* | `UserSettings`, `UserRegistry` | none |
| S9 | Manual inventory changes | sw/manual_inventory_changes.* | `ManualInventoryChanges` | none |
| B1a | Alarm severity | B/behaviour_hiding/alarm_severity.* | `AlarmSeverity` | alarm_severity_test (8) |
| B1b | Alarm resolution | B/behaviour_hiding/alarm_resolution.* | `AlarmResolution` | alarm_resolution_test (39) |
| B2a | Report types | B/behaviour_hiding/report_types.* | `ReportTypes` | report_types_test (26) |
| B2b | Report formatting | B/behaviour_hiding/report_formatting.* | `ReportFormatting` | report_formatting_test (10) |
| B3a | Inventory record | B/behaviour_hiding/inventory_record.* | `InventoryRecord` | inventory_record_test (36) |
| B3b | Low stock rule | B/behaviour_hiding/low_stock_rule.* | `LowStockRule` | low_stock_rule_test (9) |
| B3c | Hospital inventory | B/behaviour_hiding/hospital_stock.* | `HospitalStock` | hospital_stock_test (30) |
| B4a | ADC heartbeats | B/behaviour_hiding/adc_heartbeats.* | `AdcHeartbeats` | adc_heartbeats_test (19) |
| B4b | Maintenance | B/behaviour_hiding/maintenance.* | `Maintenance` | maintenance_test (38) |

## 2. Deliverable 1 models and the code that realizes them

| D1 model | Realized by | Notes |
|----------|-------------|-------|
| Activity 1: pharmacist getting data for restock | `InventoryRecord::items`, `count` (B3a); `HospitalStock::all`, `check` (B3c); `LowStockRule::check` (B3b); `ReportTypes::summary` with `medication=` filter (B2a) [CODE] | Partial [INFERRED]: the data exists, but there is no dashboard screen. The console demo commands `stock` and `hospital` stand in. Restocking from the reserves (a transfer) is not built [CODE `B/MODULE_NOTES.md` §9]. |
| Activity 2: alarm handling, acknowledgement, resolution, escalation | `AlarmResolution::add`, `acknowledge`, `resolve`, `escalateDue` (B1b); `AlarmSeverity::severityOf` (B1a); `AlarmNotifier::raise`, `recipientsFor` (S7) [CODE] | Realized in B1b. S7 still has its own 6-minute escalation (`checkUnresolved`) that the proposal retires [CODE `B/INTEGRATION_PROPOSAL.md` §1]. |
| Activity 3: tracking ADC status and the offline case | `AdcHeartbeats::heartbeatReceived`, `checkAll` (rows A–D) (B4a); `Maintenance::isInMaintenance` (B4b); `ReportTypes` flagged ADCs (B2a) [CODE] | Realized. The demo's `scenario offline` and `scenario maintenance` run it. |
| Activity 5: pharmacist accessing records | `Authorization::login`, `authorize` (S4a); `ReportTypes::summary` + audit entry (B2a); `ReportFormatting::formatSummary` (B2b); `FileAuditTrail::search` (H1b) [CODE] | Partial: B2a reads through the `HistoryReader` interface, which is still a fake. The history-reader adapter onto `FileAuditTrail` is not written. |
| State machine 1: operational | not realized as one state machine in the HMS [INFERRED] | `HMSystem::tick` (S4b) is the running loop. There is no HMS mode enum (start-up / running / shut-down). Subgroup A's `SystemState` is the ADC's, not the HMS's [CODE `Subgroup_A/inc/SystemModes.hpp`]. |
| State machine 2: alarm handling | `enum class AlarmState { Raised, Acknowledged, Resolved }` plus the transition table T1–T3 in `alarm_resolution.cpp` (`next`) [CODE] | Escalation is a flag (`escalated`), not a state, and changes the existing alarm (TODO for Inodh at `alarm_resolution.cpp:142`). |
| State machine 3: maintenance scheduler | `Maintenance::WindowState { Scheduled, Active, Completed, Cancelled }`, `WindowEvent`, and the transition table in `maintenance.cpp` (`next`, `advanceTo`) [CODE] | Inodh's S1 has a second, smaller machine: `MaintenanceWindow::State { Pending, Active, Finished }`. |
| State machine 4: ADC communication and health lifecycle | `enum class AdcStatus { Online, Offline, NotMonitored }` and the `checkAll` table rows A–D in `adc_heartbeats.cpp` [CODE] | "Decommissioned" is NotMonitored (`stopMonitoring`). "In maintenance" is not a status here; it comes from B4b. |

## 3. How each module hides its secret

| ID | Secret [SPEC brief §3, CODE header comments] | How the code hides it |
|----|------------------------------------|----------------------|
| B1a | Type-to-severity table | A single `switch` in `severityOf`; callers see only `AlarmType` and `Severity`. |
| B1b | Alarm lifecycle, note rule, escalation limits, supervisor chain | Private map of alarms, a private transition table, and constants in the `.cpp` (`CRITICAL_ESCALATION_SECONDS`, `HIGH_ESCALATION_SECONDS`, `supervisorOf`). Callers get copies (`find`) and verdicts. |
| B2a | When the daily report is due, its window, its sections, the "needs attention" rule, the audit rule | Constants and `sectionOf` rule list in `report_types.cpp`. It returns data (`DailyReport`, `Summary`) with no layout. |
| B2b | Layout and date conversion | All text building is private to `report_formatting.cpp`; date maths is done by hand, with no library. |
| B3a | How counts are stored; which changes are valid | Private `std::map<(ADC, medication), Stock>`; `items()` returns copies. |
| B3b | The 25% threshold and the "exactly 25% is not low" rule | One constant `LOW_STOCK_PERCENT` and one comparison in `low_stock_rule.cpp`. |
| B3c | How hospital totals are computed; where reserves are kept | Private reserve map; totals computed on each call from B3a's read-only queries. |
| B4a | 60 s period, 3 missed periods, tracking by "last heard" time | Constants in `adc_heartbeats.cpp`; private record per ADC; callers see only `AdcStatus`. |
| B4b | Window storage, lifecycle, 12 h limit, overlap rule | Private window list and transition table; other modules see only the `MaintenanceStatus` interface. |
| All B | Who may do what | No module holds a role rule. Each asks `Permissions::mayPerform(role, action)` [CODE `interfaces/permissions.hpp`]. One exception: B1b's supervisor chain is a role rule inside the module. |
| H1 | File format, encryption at rest, sequence numbers, archiving | All behind Inodh's `AuditTrail` interface [CODE `hw/long_term_storage.hpp`]. |
| H2, H3, H3c, H4 | OS clock, console output, console input, transceiver | Each behind one interface (`Clock`, `DashboardOutput`, `UserInputHandler`, `CommsLink`). |
| S-branch | Design choices (message format, encryption, sessions, permissions table) | Each module's `Hides:` header comment states its secret [CODE sw/*.hpp]. |

## 4. What is connected and what runs on fakes

| Link | Status |
|------|--------|
| Behaviour hiding ↔ its interfaces | Connected. All nine modules run on the interfaces in `B/behaviour_hiding/interfaces/`. |
| Behaviour hiding ↔ software decisions | **Not connected.** No adapter exists yet. The two sides use different `Clock` (seconds vs text), "no ADC" (0 vs -1), alarm type (enum vs text code), role enum and history types. `AlarmSeverity` and `ReportTypes` clash as names if compiled together [CODE `B/INTEGRATION_PROPOSAL.md` §2]. |
| Hardware hiding ↔ software decisions | Connected by type: Matthew's classes implement Inodh's interfaces and compile. Nothing constructs `HMSystem` with them, so it is not run end to end [CODE `hw/MODULE_NOTES.md`]. |
| Hardware hiding ↔ behaviour hiding | Not connected. `SystemClock` returns text; behaviour hiding's `Clock` returns seconds. |
| Comms link ↔ ADC (Subgroup A) | Stub. `HW1::Receive_Message` always returns `"HMS_ACK"` [CODE `Subgroup_A/src/HW1_Transceiver.cpp`]. |

Fakes used by behaviour hiding's tests and demo [CODE `B/tests/fakes/`]:

| Fake | Stands in for |
|------|---------------|
| `FakeClock` | the H2 clock in seconds |
| `FakeHistory` | the H1 history writer and reader (can fail on demand) |
| `FakeAdcRegistry` | the S3 registry adapter |
| `FakeHeartbeatCounter` | the heartbeat counts (owner to agree) |
| `FakeReportSender` | sending the daily report to a dashboard |
| `FakePermissions` | the S8a user-settings adapter, with the agreed table |
| `FakeAlarmNotifier` / demo `DemoNotifier` | the S7 alarm notification adapter |
| `FakeMaintenanceStatus` | B4b, for B4a's unit tests only |

Real in the demo: all nine behaviour modules. `DemoNotifier` passes raised alarms straight to the real `AlarmResolution`.
