# GUI_PLAN: one program with a GUI that runs every available HMS tool

What this is: the Phase 3A plan (plan only, no code) for an integrated demo GUI, written 2026-11-06.
How to read it: §1 what exists per module; §2 how to combine it without editing teammates' files; §3 GUI options and the recommendation; §4 screens; §5 build steps with time, risk and cut order; §6 what Andrew must decide.
Module IDs are those in CODE_MAP.md. Tags: [CODE] read in the named file; [INFERRED] my reading.
Toolchain checked today: MSYS2 UCRT64, g++ 15.2.0 (posix threads, `std::thread` works), Winsock (`-lws2_32`) works, Edge and Chrome installed. No GUI or HTTP library is installed (no wxWidgets, GTK, Qt, SDL, FLTK, ImGui, httplib).

## 1. What is available for each module

"Real" = code exists and can be linked into the GUI. "Fake" = the GUI would use a stand-in. Where real code exists but is not used, the reason is given.

| ID | Module | Available | Use in the GUI |
|----|--------|-----------|----------------|
| H1 | Long-term storage | real (`FileAuditTrail`) | Real, step 5: history written to an encrypted file, read back by history search. Until then `FakeHistory`. |
| H2 | Clock | real (`SystemClock`, text, real time) | Not used: the demo must be able to advance time. One simulated clock in seconds; a text adapter feeds Inodh's code. |
| H3 | Dashboard manager | real (`ConsoleDashboardManager`, prints) | Replaced by the GUI. A new `DashboardOutput` adapter collects each user's messages for an inbox panel. |
| H3a/b | Dashboard content / formatting | missing | The GUI screens are these. |
| H3c | User inputs | real (`ConsoleInputHandler`) | Replaced by GUI forms. |
| H4 | Comms link | real, over a stub | Not used: Subgroup A's stub always returns `"HMS_ACK"`, so `pollLink` would never return (OPEN_QUESTIONS 14). A simulated link fed by the GUI's "ADC message" box replaces it (step 4). |
| S1 | Maintenance scheduler | real | Not used: duplicates B4b with text times and different rules (X6, X7). Shown as "real, not wired". |
| S2 | Report generation | real | Not used: its `ReportTypes` interface is replaced by B2a (proposal §5). Its `EventRecorder` role is taken by an adapter onto the shared history. |
| S3 | ADC registry | real (`ADClist`) | Real, step 3: the ADC list screen (register, relocate, remove), via a registry adapter. |
| S4a | Authorization | real | Real, step 2: login, lockout, 15-min session timeout (on the simulated clock). |
| S4b | Initialize (`HMSystem`) | real | Not used as a whole: it needs every interface at once, including the clashing `ReportTypes`. The GUI does its own wiring in a new file. |
| S4c | Daily tasks | real | Optional, step 5: archiving with the real storage. The daily report is triggered by B2a directly. |
| S5a | Encryption | real | Real, step 4 (message path) and step 5 (storage). |
| S5b | Patient info stripping | real | Real, step 4: the GUI shows the message before and after stripping. |
| S6a/b/c | Event contents / formatting / routing | real | Real, step 4: typed ADC messages go through the real router. |
| S7 | Alarm notification | real | Partly, step 4: only as the router requires it. Alarm state stays in B1b (proposal §1). Two alarm lists remain until Inodh's `raise` calls `add` (known gap). |
| S8a | User settings | real | Real, step 2: login roles. Optional toggle to use it as the permission table (it differs from the agreed one, I3). |
| S8b | User registry | real | Real, step 2: users screen (add, remove, unlock) for administrators. |
| S9 | Manual inventory changes | real | Not used: no explanation (FR-INV-04) and duplicates B3a. |
| B1a–B4b | All nine behaviour modules | real, tested | Real from step 1 (as in `demo_main.cpp`). |
| — | Permissions | fake (agreed table) | Fake by default; step 2 adds an adapter onto S8a as an option. |
| — | Heartbeat counter, report sender | fake | Fake: the simulated ADCs log heartbeats; the sender keeps sent reports for the reports screen. |

## 2. Combining the code without editing teammates' files

Everything new goes in `Subgroup_B/gui/` (and `Subgroup_B/third_party/` for the HTTP library). No existing file changes, except one new `gui` target appended to the makefile (see §6).

**Wiring.** One new `gui/hms_world.*` builds everything (like `World` in `demo_main.cpp`) around one simulated clock in seconds. One mutex guards it, because the HTTP library answers requests on several threads.

**Adapters** (new files in `gui/adapters/`, each a few lines; proposal §6 lists most of them):

| Adapter | Makes | Out of |
|---------|-------|--------|
| Text clock | Inodh's `Clock::now()` text | the simulated seconds, via `ReportFormatting::formatTime` (inverse of `timeutil::toSeconds`) |
| Registry | B's `AdcRegistry` | `ADClist::exists`, `all` |
| Permissions (optional) | B's `Permissions` | `UserSettings::isAllowed`, mapping roles by name (the two `Role` enums are in different orders) |
| Event recorder | Inodh's `EventRecorder` | B's `History`, so his events appear in history search |
| History writer / reader (step 5) | B's `History`, `HistoryReader` | `FileAuditTrail::append`, `search`. Catches its exceptions and returns `StorageFailure`; maps 0 to -1; turns `[from, to)` into his inclusive range. |
| Heartbeat monitor | Inodh's `HeartbeatMonitor` | `AdcHeartbeats::heartbeatReceived` plus the heartbeat log |
| Inventory store | Inodh's `InventoryStore` | `InventoryRecord`: `setCount` becomes `deposit` or `withdraw` of the difference. An unknown medication is refused and shown. |
| Severity rules | Inodh's `AlarmSeverityRules::classify` | `AlarmSeverity::severityOf` (code to type; unknown code means Critical) |
| Alarm resolver | Inodh's `AlarmResolver` | a small table of his alarm ids |
| Dashboard output | Inodh's `DashboardOutput` | per-user inbox shown in the GUI |
| Simulated comms link | Inodh's `CommsLink` | a queue the GUI fills, encrypted with `EncryptionManager::encryptForADC` |

**Name clashes** (`AlarmSeverity`: Inodh's enum vs Andrew's class; `ReportTypes`: Inodh's interface vs Andrew's class):
- It is not only a compile problem. Both `ReportTypes` would emit an inline destructor `ReportTypes::~ReportTypes()` with the same linker name, and the linker would silently keep one [INFERRED from how GCC emits inline functions].
- **Proposed fix:** compile Inodh's `.cpp` files for the GUI with `-DAlarmSeverity=SwAlarmSeverity -DReportTypes=SwReportTypes`. This renames his two types in his code only, at compile time, without touching his files.
- Adapter files that need both sides include a new `gui/adapters/sw_names.hpp`. It defines the two macros, includes Inodh's headers, then `#undef`s them before Andrew's headers are included.
- After the first link, `nm` checks that no symbol is defined twice.
- The lasting fix is namespaces (proposal §2), which needs the owners to agree.

## 3. GUI options (C++17, MinGW, no admin installs)

| Option | Pros | Cons |
|--------|------|------|
| **A. Local web page + cpp-httplib** (your default). One MIT-licensed header in `third_party/`; the C++ program serves a page and a small JSON API on `127.0.0.1` only; the browser shows it. | Real tables, filters, colours and forms with little code; HTTP handling is a well-tested library; the GUI layer is just one more "hardware hiding" boundary; screenshots for the report are easy. | Needs a one-time download of `httplib.h` (no network at run time); about 10k lines of third-party code; slower compile of that one file; needs a browser (Edge and Chrome are installed). |
| **B. Local web page + own tiny Winsock server** (~200 lines in `gui/`) | No download; every line can be explained to a TA; same page as A. | We own the HTTP parsing and its bugs; one request at a time; more of our time on plumbing instead of screens. |
| **C. Native Win32 window** (`windows.h`, common controls) | Nothing to download or install; a real desktop window. | Very verbose; tables, filters and colours are slow to build; the window code would dwarf the HMS code. |
| **D. Dear ImGui + Win32/DirectX 11** | Quick to build many screens; one window; DX11 libraries are present. | Needs about 10 files downloaded; immediate-mode code is unfamiliar to explain; accessibility is poor (no screen-reader support). |
| E. Qt or wxWidgets via `pacman` | Full toolkits. | Large downloads and installs; heavy builds; overkill for a demo. Rejected. |

**Recommendation: A.** It produces the most screens for the least risk, and HTTP handling is the part most likely to fail during a live demo, so it's best left to a tested library. If you'd rather not add third-party code, choose B: the page and JSON API are identical, so only the server file changes.

Either way:
- The page is plain HTML, CSS and JavaScript in `gui/web/`, served from disk. There are no CDN links, fonts or frameworks, so there are no network calls at run time.
- The server listens on `127.0.0.1` only.
- Every action returns a success or failure message, showing the module's own verdict name (for example "Rejected: NoExplanation").

## 4. Proposed screens

| Screen | Contents | Backed by |
|--------|----------|-----------|
| Login and role switch | Log in as a seeded user (wrong passwords count towards the 4-failure lockout; locked accounts say so); a "quick switch" to Technician, Pharmacist or Administrator for the demo; who is logged in; session timeout shown. | S4a, S8a, S8b (step 2); before that, a role picker like the demo's `user` command |
| Dashboard | ADC rows (status Online, Offline or Not monitored, in-maintenance flag, last heartbeat); open alarms with filters (severity, state, ADC, type); severity as **text and colour** (Critical red, High amber, Low grey-blue, never colour alone); clock; the user's inbox. | B4a, B4b, B1b, dashboard adapter |
| Alarms | Full list; acknowledge; resolve with a required note (a blank note shows "Rejected: NoExplanation"); escalation status; raise a test alarm. | B1b, B1a |
| Inventory | Per-ADC stock with low flags; hospital totals and reserves with their alarm; withdraw, deposit, manual change (explanation required), reserve change. | B3a, B3b, B3c |
| Maintenance | Windows per ADC with state; schedule, cancel; verdicts shown. | B4b |
| ADCs and users | ADC list: register, relocate, remove (Administrator). Users: add, remove, unlock (Administrator). | S3, S8b (steps 2–3) |
| Reports | Daily report now (text from B2b); activity summary form (ADC, medication, actor, from, to); refused requests show their verdict, and the audit entry is visible. | B2a, B2b |
| History search | Time range plus filters over the history; each row shows date, time, who, kind and detail. | B2a reader over `FakeHistory` (step 1) or `FileAuditTrail` (step 5) |
| ADC message (step 4) | Type a raw ADC message (for example `1|Withdrawal|Morphine|5|U1|PT_NAME=Jane Doe`); see it encrypted, decrypted, stripped (patient field gone), routed, and the effect. | S5a, S5b, S6a–c, adapters |
| Simulation controls | Advance the clock (10 s, 1 min, 3 min, 10 min, to 06:00); silence or resume an ADC; send one heartbeat; make history or report sending fail; scenario buttons (offline, maintenance, low stock, over-withdrawal, escalation); **Restart** (fresh world). | the fakes, as in `demo_main.cpp` |

## 5. Build steps, time and risk

Each step ends with `mingw32-make`, `mingw32-make test` (215) and `mingw32-make gui` passing. Times are my estimate of working time.

| Step | What | Time | Risk | Main risk |
|------|------|------|------|-----------|
| 0 | `third_party/httplib.h`, `make gui`, a page that says hello on `127.0.0.1:8080` | 0.5 h | Low | httplib needs `-lws2_32` and a Windows 10 target define on MinGW |
| 1 | World wiring, a small hand-written JSON writer, all behaviour screens: dashboard, alarms, inventory, maintenance, reports, history search (FakeHistory), simulation controls, scenarios, restart | 3–4 h | Low–medium | thread safety (one mutex); keeping the page simple |
| 2 | Real login, lockout, sessions, users screen; optional real permission table | 1.5 h | Medium | first use of the rename shim; text clock; role enum order |
| 3 | Real ADC registry screen | 1 h | Low | registry vs heartbeats monitoring kept in step |
| 4 | Real message path: link, encryption, stripping, scanner, router, Inodh's notifier | 3 h | High | router needs Inodh's concrete notifier and all its parts; `setCount` mapping; two alarm lists; unknown-medication deposits |
| 5 | Real encrypted storage as history; optional archiving | 1.5 h | Medium | exceptions; mapping history kinds onto `EventType` (kind kept in `fields`); inclusive ranges; files go in `bin/gui_data/` (already git-ignored) |
| 6 | Polish: labels, keyboard focus, contrast, consistent messages, run instructions | 1 h | Low | — |

Total about 11–13 h. **Cut first:** step 4, then step 5, then step 3, then step 2. Step 1 alone already demonstrates every behaviour module with a GUI. Steps 1 and 2 together come to about 5–6 h.

## 6. What I need from Andrew

1. **Approve the plan** and the option (A recommended; B if no third-party code).
2. **For A: the HTTP library.** Either you download one pinned cpp-httplib release (`httplib.h`, MIT licence) into `Subgroup_B/third_party/`, or you allow me one download of it. I'll pin a release at least a few weeks old, record its version and licence in `third_party/README.md`, and nothing will fetch anything at run time.
3. **The makefile.** It is shared and Matthew changed it last. May I append a `gui` target at the end, changing nothing else? (The alternative is a separate `gui/makefile`, run with `mingw32-make -C gui`.)
4. **The rename shim** for Inodh's two type names in the GUI build (§2). It doesn't edit his files, but he should know. OK?
5. **Scope and deadline.** Which steps (the default is 0–3, then 4–5 if time allows), and when is the demo? The design document is due tonight.
6. **Demo accounts** for login (for example `tara`/Technician, `priya`/Pharmacist, `alex`/Administrator, with demo passwords), and the port (default 8080).
7. **Permission table in the GUI:** the agreed table (as the tests use), Inodh's `user_settings`, or a toggle between them?
