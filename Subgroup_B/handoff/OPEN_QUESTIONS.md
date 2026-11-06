# OPEN_QUESTIONS: what is still undecided, and who must answer

What this is: every open question found in the code, the specs and the team's notes, as of 2026-11-06.
How to read it: numbered list; each item says who must answer (Andrew, Inodh, Matthew, or the team), and where the question comes from.
Questions 1–11 continue the list in `Subgroup_B/INTEGRATION_PROPOSAL.md` §7 (same order, re-worded); the rest are new from this survey.
Tags: [CODE] read in the named file; [SPEC] Andrew's brief `HMS_behaviour_modules_brief.md`; [INFERRED] my reading.
IDs (B1b, S7, ...) are the module IDs in CODE_MAP.md; A-, I-, M- and X-numbers refer to ASSUMPTIONS_AND_DECISIONS.md.

## From the integration proposal

1. **"All ADCs" maintenance windows.** Inodh's scheduler has them (`kAllAdcs = -1`); Andrew's Maintenance does not. Keep them? **Inodh and Andrew.** [CODE sw/maintenance_scheduler.hpp]
2. **Maintenance starting in the past.** Inodh rejects it; Andrew allows a window already under way. Which wins? **Inodh and Andrew.** (X6)
3. **Alarm recipients.** Inodh sends Critical alarms to two roles; alarm resolution holds one destination per alarm. One role or a list? **Inodh.** [CODE sw/alarm_notification.cpp:29-36]
4. **Exceptions.** Encryption, `getUserRole`, `generateEncrypted`, startup checks and Matthew's storage all throw. Is "return a verdict, never throw while running" a team rule? Who catches storage exceptions? **Team.** (I24, M2)
5. **Escalation form.** The spec says escalation creates a new alarm; alarm resolution changes the existing one. **Inodh.** [CODE B/alarm_resolution.cpp:142] (X2)
6. **Offline alarm type.** Heartbeats raise `AdcOffline` (Critical), not `ConnectionLoss` (Low). Confirm. **Inodh.** [CODE B/adc_heartbeats.cpp:68] (X11)
7. **Discrepancy severity.** Is High the severity intended for `DISCREPANCY`? **Inodh.** (A29)
8. **Transfers.** Should an ADC deposit take stock out of the pharmacy reserves? Should deliveries into the reserves be built? **Team.** (A10)
9. **"No ADC" value.** 0 (behaviour) or -1 (software decisions)? **Team.** (X10)
10. **Moving `sw_decisions/` into `Subgroup_B/`.** When, and by whom (keeping its git history)? **Team; Andrew runs git.**
11. **Module tree file.** Add `hospital_stock` (Hospital inventory) under `behav_hiding/inventory_manager/` in `module_diagram.txt`. **Whoever owns that file (Inodh wrote it [INFERRED]).**

## New from this survey

12. **Missing specs.** `Subgroup_B/doc_inputs/` does not exist, and no D1 or requirement document is in the repo. Please add the requirement text and the D1 models so the [INFERRED] summaries in TRACEABILITY.md can be checked. What is "Section 2" (named in the task for ASSUMPTIONS_AND_DECISIONS.md)? **Andrew.**
13. **Unknown requirements.** What do FR-ALM-05, NFR-SAF-03, NFR-PRF-01, NFR-PRF-03, NFR-USE-02 and NFR-USE-03 require? Every other [INFERRED] summary in TRACEABILITY.md also needs checking against D1. **Andrew (from D1).**
14. **Comms link would hang the main loop.** `HW1::Receive_Message()` always returns `"HMS_ACK"` [CODE Subgroup_A/src/HW1_Transceiver.cpp]. `HardwareCommsLink::receive()` treats any non-empty string as a packet, and `MessageRouter::pollLink` loops until `receive()` is empty [CODE sw/events_logs/message_routing.cpp:106-113]. Wired together, `HMSystem::tick()` would never return [INFERRED]. Also, the packet bytes are plain text, but the router expects encrypted packets. How should "no message" and encryption work on the link? **Matthew and Inodh.** (M5)
15. **Two clocks.** Matthew's `SystemClock` returns text (Inodh's interface); behaviour hiding's `Clock` returns seconds. Who writes the seconds clock or the adapter? **Matthew.** (A1, M1)
16. **Severity rules implementation.** Nothing in the repo implements Inodh's `AlarmSeverityRules::classify`. The proposal says an adapter over Andrew's `AlarmSeverity` does it; who writes it? **Andrew and Inodh.** [CODE sw/external_interfaces.hpp]
17. **Escalated alarms leave `pending()`.** `AlarmNotifier::checkUnresolved` erases an escalated alarm from `pending_` even if it is still unresolved [CODE sw/alarm_notification.cpp:85]. Is that intended, given "an alarm is never discarded before it is acknowledged" (NFR-SAF-02)? **Inodh.** (I22)
18. **Permission table.** Maintenance: Technician (proposal) or Administrator (`user_settings.cpp`)? Add `AcknowledgeAlarm` and `CancelMaintenance`, and Administrator to manual inventory changes? **Inodh and Andrew.** (I3, X5)
19. **Manual change explanation.** `ManualInventoryChanges::apply` takes no explanation, but FR-INV-04 needs one. Add it, or route through `InventoryRecord::manualChange`? **Inodh.** (I16, X8)
20. **Zero quantity and unknown deposits.** `event_contents` lets a quantity of 0 through, and the router creates an item on a deposit of an unknown medication. Inventory record rejects both. Which rule wins? **Inodh and Andrew.** (I14, I15, X13)
21. **Duplicate-alarm match includes detail.** FR-ALM-04 says same ADC and type; the code also needs the same detail (so two medications are two alarms). Accept this reading? **Andrew, then tell the team.** (A25, X4)
22. **Malformed messages raise no alarm.** `IncorrectMessageFormat` is in the severity table, but the router just drops malformed messages (`Outcome::Malformed`). Should it raise the alarm? **Inodh.** [CODE message_routing.cpp:62-80]
23. **Dashboard scope.** FR-DSH-01..04 have only a console `deliver`. Is Matthew building dashboard content and formatting (H3a, H3b), or does the planned GUI cover them? **Matthew and Andrew.**
24. **Configuration (FR-CFG-01).** Every threshold and period is a named constant in code. Is run-time configuration required? **Team (check D1).**
25. **Tests for teammates' code.** No tests for `sw_decisions/` or `hardware_hiding/` are in the repo. Matthew's notes mention an encrypted-storage test that passed. Should those tests be added to `Subgroup_B/tests/`? **Inodh and Matthew.**
26. **Out-of-date notes.** `PROGRESS.md` still says "Next: hw_hiding". Update it? **Inodh.** (X15; the proposal's makefile note was fixed 2026-11-06.)
27. **Stray folder.** `Subgroup_B/-p/` is an empty folder dated 2026-11-04 18:23, likely from a `mkdir -p` run on Windows [INFERRED]. Delete it? **Andrew.**
28. **Who builds the GUI front end, and on which interfaces?** The brief says the demo front end is undecided [SPEC brief §7]. This affects Phase 3. **Andrew.**

## Settled since the first version

- **Invalid alarm actions and the log** (was question 21): decided by Andrew on 2026-11-06. They return a verdict, change nothing and write no log entry, because logging every rejected action would flood the audit trail. This is a deliberate departure from the Week 6 slide and is easy to change. See A24 and X3 in ASSUMPTIONS_AND_DECISIONS.md.
