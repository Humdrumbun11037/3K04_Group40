# HMS demo GUI

A local web page that drives the nine real behaviour-hiding modules. It is built separately from the shared makefile and uses no teammate code.

## Build and run (MSYS2 / MinGW, from `Subgroup_B/gui`)

1. Put a pinned `httplib.h` (cpp-httplib) in `Subgroup_B/third_party/`. Nothing is downloaded by the build or at run time.
2. Use these commands:

| Command | What it does |
|---------|--------------|
| `mingw32-make test` | Builds and runs the API tests. Works without `httplib.h`. |
| `mingw32-make run` | Builds the server and starts it. Then open **http://127.0.0.1:8080**. Ctrl+C stops it. |
| `mingw32-make` | Builds the server only. |
| `mingw32-make clean` | Deletes `gui/obj` and `gui/bin`. |

The server listens on 127.0.0.1:8080 only and refuses requests addressed to any other host name. The page loads nothing from outside.

## What is real and what is fake

- **Real:** alarm severity, alarm resolution, report types, report formatting, inventory record, low stock rule, hospital stock, ADC heartbeats and maintenance.
- **Fake** (from `tests/fakes/`, same as the console demo):
  - the clock (simulated and advanced by buttons)
  - history (in memory, and can be made to fail)
  - the ADC registry
  - the heartbeat counter
  - the report sender (keeps sent reports, and can be made to fail)
  - the permission table (the agreed table only)
  - alarm notification (`GuiNotifier`, which uses the console demo's guesses for the destination roles)
- **Simulated ADCs:** one heartbeat a minute each unless silenced. The periodic checks run every 10 simulated seconds.

## Files

| File | Role |
|------|------|
| `hms_world.*` | Wiring of the real modules and the fakes, the simulated ADCs and the activity feed |
| `api.*` | Each request name, the module call it becomes, and its JSON answer, including the module's verdict |
| `json.hpp`, `names.hpp` | JSON writing; display names for the enum values |
| `server_main.cpp` | The only HTTP code (cpp-httplib) |
| `web/` | The page (`index.html`, `style.css`, `app.js`) |
| `tests/api_test.cpp` | 24 assert-based tests of the API, with no HTTP involved |

## Integrated mode (all three codebases)

A second program with its own state. The simulation mode above is unchanged.

| Command (from `Subgroup_B/gui`) | What it does |
|---------|--------------|
| `mingw32-make integrated-test` | Builds and runs the integrated API tests. Works without `httplib.h`. |
| `mingw32-make run-integrated` | Builds and starts the integrated server. Then open **http://127.0.0.1:8081**. |
| `mingw32-make check-symbols` | Checks that the renamed types (see `integrated/sw_names.hpp`) are not defined on both sides. |

**Demo accounts.** They live in Inodh's user registry; 4 wrong passwords in a row lock an account.

| User | Password | Role |
|------|----------|------|
| tara | tara-demo | Technician |
| priya | priya-demo | Pharmacist |
| alex | alex-demo | Administrator |

The page's **Components** tab lists which parts are REAL and which are SIMULATED.

**Data.** The integrated mode keeps its encrypted audit trail in `gui/data/` (`active.log`, `archive.log`, and the key in `key.bin`). The folder is git-ignored by `gui/.gitignore`. Each start and each Restart begins a fresh trail; the key is kept. The tests use `gui/data/test/`.

**Adapters** (all in `gui/integrated/`, none in teammates' files):

| Adapter | Connects | Converts |
|---------|----------|----------|
| `sw_names.hpp` + `SW_RENAME` in the makefile | Inodh's and Matthew's code with Andrew's | Renames their `AlarmSeverity`/`ReportTypes` to `SwAlarmSeverity`/`SwReportTypes` |
| `convert.hpp` | Shared types | Seconds and text time; 0 and -1 for "no ADC"; `Role` and `UserRole` by name |
| `TextClock` | Inodh's `Clock` | The simulated seconds clock, as text |
| `RegistryAdapter` | Andrew's `AdcRegistry` | Inodh's `ADClist` |
| `EventToHistory` | Inodh's `EventRecorder` | His `Event` into Andrew's `HistoryEntry` |
| `HeartbeatLink` | Inodh's `HeartbeatMonitor` | Into `AdcHeartbeats::heartbeatReceived` |
| `InventoryLink` | Inodh's `InventoryStore` | `setCount` into a deposit or withdrawal of the difference |
| `Inbox` | Inodh's `DashboardOutput` | Per-user messages shown on the page |
| `SeverityRules`, alarm code table | Inodh's `AlarmSeverityRules` | Text code into `AlarmType`, then Andrew's severity table |
| `AlarmLinks`, `ResolverLink`, `NotifierBridge` | Inodh's `AlarmNotifier` and Andrew's `AlarmResolution` | His alarm ids into Andrew's alarms; resolved state back to him |
| `FileHistory` | Andrew's `History` and `HistoryReader` | Matthew's `FileAuditTrail`: thrown exceptions into `StorageFailure`; `[from, to)` into his inclusive range |
