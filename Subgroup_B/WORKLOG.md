# WORKLOG: handoff, comments and GUI

A fresh session can resume from this file. Phases run one at a time; after each
phase Claude stops and waits for Andrew to say "continue".

Rules: no git; create or edit files only inside Subgroup_B; never edit
Subgroup_A, sw_decisions/ or a teammate's file (Matthew's hardware_hiding/ is
his); `mingw32-make test` must pass after every phase; if bin/demo.exe is
locked, say so and don't stop the process.

| Phase | What | Status |
|-------|------|--------|
| 1 | Survey; write handoff/CODE_MAP.md, IMPLEMENTATION_NOTES.md, TRACEABILITY.md, ASSUMPTIONS_AND_DECISIONS.md, OPEN_QUESTIONS.md | Done |
| 1b | Correction pass: (a) refresh handoff files from doc_inputs/; (b) proposal makefile note; (c) record old question 21 as a decision | (b), (c) done; (a) waiting for doc_inputs/ |
| 2 | Comments in behaviour_hiding/*.hpp/*.cpp and demo_main.cpp (comments only); handoff/COMMENT_SUGGESTIONS.md for teammates | Done; waiting for "continue" |
| 3A | Plan the integrated GUI (plan only) | Done: handoff/GUI_PLAN.md; waiting for approval and answers to its §6 |
| 3B | Build the GUI: Step 0 + Step 1 only (Option A, gui/makefile, port 8080, no teammate code) | Step 1 code done and tested (24 API tests); server not yet built: waiting for third_party/httplib.h |

## Log

- 2026-11-06: Created this file. Baseline `mingw32-make test`: 215 tests pass
  (adc_heartbeats 19, alarm_resolution 39, alarm_severity 8, hospital_stock 30,
  inventory_record 36, low_stock_rule 9, maintenance 38, report_formatting 10,
  report_types 26). Found since last session: Matthew's hardware_hiding/ and a
  changed makefile (both 08:34 today). Subgroup_B/doc_inputs/ is missing: asked
  Andrew where the specs are.
- 2026-11-06: Andrew said "proceed without specs". [SPEC] tags cite Andrew's
  brief C:\Users\andre\Downloads\HMS_behaviour_modules_brief.md (outside the
  repo); requirement summaries not in it are [INFERRED].
- 2026-11-06: Phase 1 done. Wrote handoff/CODE_MAP.md (206 lines),
  IMPLEMENTATION_NOTES.md (91), TRACEABILITY.md (62), ASSUMPTIONS_AND_DECISIONS.md
  (142), OPEN_QUESTIONS.md (29 questions). No code edited. `mingw32-make test`:
  215 pass. All 15 sw_decisions .cpp files pass -fsyntax-only; `mingw32-make all`
  builds hardware_hiding too. Nothing outside Subgroup_B changed.
- 2026-11-06: Correction pass. Subgroup_B/doc_inputs/ (D1_requirements.md,
  Section2_specs.md) still missing; Andrew chose "Phase 2 now, (a) later".
  (b) INTEGRATION_PROPOSAL "Where things stand" now says the makefile also
  builds hardware_hiding while the tests link only behaviour_hiding.
  (c) Old OPEN_QUESTIONS 21 recorded as decision A24/X3 (invalid alarm actions:
  verdict, no change, no log entry; departs from the Week 6 slide; easy to
  change); later questions renumbered 21-28; MODULE_NOTES alarm resolution
  assumptions updated.
- 2026-11-06: Phase 2 done. 98 comment lines added to 17 of the 19 files
  (alarm_severity.cpp and low_stock_rule.cpp already commented; interfaces/
  is outside the scope). Verified against copies in the scratchpad
  (phase2_orig/): only added lines, all starting with //; line endings kept
  (CRLF files stay CRLF). make clean + make test: 215 pass, no warnings.
  Wrote handoff/COMMENT_SUGGESTIONS.md (sw_decisions + hardware_hiding).
- Still to do for (a), when doc_inputs/ exists: replace every [INFERRED]
  requirement summary in TRACEABILITY, ASSUMPTIONS_AND_DECISIONS,
  OPEN_QUESTIONS and IMPLEMENTATION_NOTES with D1 text; cite Section 2 instead
  of the brief; drop answered questions; update headers. NFR-SAF-03 is new in
  D2; FR-CFG-01 replaces NFR-MNT-01.
- 2026-11-06: Phase 3A done (plan only, no code). handoff/GUI_PLAN.md:
  module availability, adapters, rename shim for AlarmSeverity/ReportTypes
  (-D macros on Inodh's TUs), options A-E (recommend A: local page +
  cpp-httplib on 127.0.0.1), screens, steps 0-6 with time/risk, cut order
  4,5,3,2, and 7 questions for Andrew. Toolchain: MSYS2 UCRT64 g++ 15.2,
  posix threads, ws2_32 OK; no GUI/HTTP library installed.
- Next: Phase 3B only after Andrew approves GUI_PLAN.md and answers its §6.
- 2026-11-06: Andrew approved 3A with changes: Option A; he supplies
  third_party/httplib.h (no downloads); separate gui/makefile (shared makefile
  untouched); Steps 0-1 only; three-role switch, no login; agreed permission
  table only; no Inodh/Matthew code and no rename trick yet.
- 2026-11-06: doc_inputs/ and third_party/httplib.h were both still missing;
  Andrew chose "build what doesn't need them". Built gui/: hms_world.*, api.*,
  json.hpp, names.hpp, server_main.cpp (compile-checked against a scratch stub
  of httplib only), web/index.html, style.css, app.js (node --check OK; no
  outside links), tests/api_test.cpp (24 pass, no warnings), makefile, README.
  Every API answer checked as valid JSON. Shared make test still 215.
- Still to do: (1) when doc_inputs/ exists, item (a) of the correction pass
  (see the earlier entry); (2) when third_party/httplib.h exists, run
  `mingw32-make` in gui/, fix any httplib build issue, start it, and try the
  page in a browser; then report Step 0/1 done. Phase 2 comments for
  behaviour_hiding were already done earlier; gui/ code is commented.
- 2026-11-06: Andrew approved connecting all three codebases as a second
  program (gui/ targets integrated-test, run-integrated; Step 1 untouched).
  BLOCKED: Subgroup_B/third_party/httplib.h and Subgroup_B/doc_inputs/ are
  still not on disk (checked the whole user folder; third time). So the
  correction-pass item (a) and building/running the Step 1 server stay blocked.
  Carrying on with steps 3, 2, 4, 5 as C++ logic plus API tests (no httplib).
- 2026-11-06: Integrated step 3 (real ADC registry) done. New: gui/integrated/
  sw_names.hpp (rename shim), convert.hpp, adapters/basic_adapters.* (TextClock,
  RegistryAdapter, EventToHistory), int_world.*, int_api.* (POST
  /api/adc/register|relocate|remove; Inodh's own table decides who may), and
  gui/tests/integrated_test.cpp (8 tests). gui/makefile: integrated-test,
  integrated, run-integrated, check-symbols; Inodh's/Matthew's files compiled
  with -DAlarmSeverity=SwAlarmSeverity -DReportTypes=SwReportTypes. Problem
  hit: the test exe died with 0xC0000139 because /mingw64/bin's older
  libstdc++-6.dll comes first on PATH; fixed by linking integrated programs
  with -static (Step 1 rules untouched). check-symbols: only eventTypeName,
  ~ADC, ~RoleSettings (all from Inodh's headers) are shared; nothing of
  AlarmSeverity/ReportTypes. The integrated page is written after step 2.
- 2026-11-06: Integrated step 2 (real login and users) done. IntWorld now holds
  Inodh's UserRegistry, UserSettings and Authorization (on the simulated text
  clock); demo accounts tara/tara-demo, priya/priya-demo, alex/alex-demo (in
  gui/README.md). API: /api/login, /api/logout, /api/users/add|remove|unlock;
  user actions need a live session (401), simulation controls don't; state
  hides medication/alarm/user data before login; Inodh's authorize() decides
  ADC-registry and user-management rights; Andrew's modules get the session
  role and the agreed table. 13 new tests (21 total, all pass). Integrated
  page (gui/integrated/web/: login, ADCs, Users, Components, ...) and
  int_server_main.cpp (port 8081) written; server compile-checked against a
  stub only (no httplib.h). Scenarios that need a user log in as tara/priya.
- 2026-11-06: Integrated step 4 (real message path) done. Simulated ADCs
  encrypt messages (Inodh's EncryptionManager, in-memory key) into per-ADC
  queues (max 20); each tick routes at most one packet per ADC with
  MessageRouter::routePacket (pollLink and Matthew's comms stub not used).
  New adapters: adapters/message_adapters.* (HeartbeatLink -> AdcHeartbeats,
  InventoryLink: setCount -> deposit/withdraw of the difference, Inbox =
  DashboardOutput) and adapters/alarm_adapters.* (code table, SeverityRules,
  AlarmLinks::sync, ResolverLink, NotifierBridge). Inodh's AlarmNotifier now
  delivers every alarm; AlarmResolution tracks it (destination = first role in
  his chart); his checkUnresolved is never called. API: POST
  /api/message/send; stock uploads go through the router. Page: ADC messages
  screen with the step-by-step trace. 14 new tests (35 total, all pass).
  Problems found: a quantity too big for int makes Inodh's router throw
  (std::stoi out_of_range; event_contents only checks digits) - caught in
  IntWorld::routeOne; ADC alarm codes outside the table (e.g. TEMPERATURE) are
  delivered but have no AlarmType, so alarm resolution can't track them.
- 2026-11-06: Integrated step 5 (real encrypted storage) done. FileHistory
  (adapters/file_history.*) puts Andrew's History/HistoryReader on Matthew's
  FileAuditTrail: entries become Events (real kind/who/detail/medication in
  fields; event type is only a carrier because EventType has no generic
  value), seconds <-> text, [from,to) -> inclusive, 0 <-> -1, every exception
  -> StorageFailure. Key from Inodh's loadOrCreateEncryptionKey(data/key.bin);
  one EncryptionManager shared by storage and the message path. Files in
  gui/data/ (gui/.gitignore); each start/restart begins a fresh trail, key kept;
  tests use gui/data/test. 7 new tests (42 total, all pass), including a real
  write failure (active.log replaced by a folder) and a corrupted record.
  Matthew's file is written in text mode, so lines end in CRLF on Windows.
- State after this session: Step 1 untouched (24 tests); integrated 42 tests;
  shared 215; check-symbols OK. STILL BLOCKED: third_party/httplib.h and
  doc_inputs/ are not on disk, so neither server has been built or opened in a
  browser, and correction item (a) is not done. Next: once they exist, build
  and run both servers (`mingw32-make run`, `mingw32-make run-integrated`),
  click through every screen, fix anything httplib-specific; then item (a).
- 2026-11-06: Andrew reported server_main.cpp failing to compile against the
  real httplib.h. Cause: third_party/httplib.h is 14 bytes containing
  "404: Not Found" (a failed download saved as the header), not cpp-httplib.
  No code changed. Servers still not built; doc_inputs/ exists but is empty.
  Tests: gui 24, integrated 42, shared 215.
- 2026-11-06: Real cpp-httplib 0.65.0 (MIT) in third_party/ (recorded in
  third_party/README.md). Both servers compiled with no code changes and no
  warnings; the earlier compile error came only from the "404: Not Found" file.
  The simulation server then died at start (exit 127: old libstdc++-6.dll on
  PATH), so gui/makefile SERVER_LIBS now adds -static. curl checks (scratchpad
  curl_check.sh): pages load on 8080 and 8081, Host evil.example refused (403)
  on both, one API call per screen returns valid JSON (33/33 after fixing a
  case-sensitive check in the script); patient name not found in active.log.
  Servers stopped. Tests: gui 24, integrated 42, shared 215. doc_inputs/ is
  still empty, so correction item (a) is still not done.
