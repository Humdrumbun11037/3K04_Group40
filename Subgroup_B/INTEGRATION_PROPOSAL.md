# Integration proposal: software decisions + behaviour hiding

From Andrew, for Inodh and Matthew. This proposes how Inodh's `sw_decisions/`
code and Andrew's `behaviour_hiding/` code fit together inside `Subgroup_B`,
with room for Matthew's hardware-hiding folder. Nothing here is decided until
we agree it. Every point is open for discussion.

## Where things stand

- `Subgroup_B/behaviour_hiding/` builds and passes its tests (`make test` in
  `Subgroup_B`). Its makefile builds **only** `behaviour_hiding` for now.
- `sw_decisions/` is still at the repo root and has not been moved or edited.
  Its headers are now pushed, and its code is in good shape:
  - **All 13 `.cpp` files compile on their own.** Each was checked with
    `g++ -std=c++17 -fsyntax-only`. No `-I` flags are needed, and there are no
    warnings even with `-Wall -Wextra`.
  - **They also link together.** Nothing is declared without being defined.
    The message router now exists (`events_logs/message_routing`).
  - **No header or type is missing.** `Event`, `EventType`, `EventRecorder`,
    `Clock`, `timeutil`, `AuditTrail`, `DashboardOutput`, `InventoryStore`,
    `AlarmSeverityRules`, `AlarmResolver`, `ReportTypes`, `HeartbeatMonitor`
    and `ExternalModules` are all defined, mostly in `external_interfaces.hpp`.
- **Two names clash when both sides are compiled together.** I checked this by
  compiling one file that includes `startup/initialize.hpp` and the behaviour
  headers:
  - `AlarmSeverity`: an enum in Inodh's code, a class in Andrew's.
  - `ReportTypes`: an interface in Inodh's code, a class in Andrew's.

  `Clock` and `AlarmNotifier` exist on both sides too, but don't clash, because
  Andrew's are inside `namespace hms::behaviour`. Putting Inodh's code in
  `namespace hms::sw` fixes both clashes (see section 2).
- Once the folder is moved into `Subgroup_B`, adding `sw_decisions` to
  `MODULE_DIRS` in the makefile is a one-word change.

## 1. Who owns what

The rule we use to decide: **software decisions route, store, deliver and
check identity. Behaviour hiding decides the rules that would change if a
requirement changed. Hardware hiding talks to devices.** Where two files
currently do the same job, one of them gives it up.

| Job | Owner | What the other side does |
|-----|-------|--------------------------|
| Maintenance windows: accepting, cancelling, start and end, "is this ADC in maintenance?" | **Behaviour** (`maintenance`) | Inodh's `maintenance_scheduler` is retired, or becomes a thin front end that turns dashboard input (text times) into a call to `Maintenance::schedule`. Both sides already use the same 12-hour limit. |
| Escalation timing (Critical 2 min, High 10 min, Low never) and the supervisor chain | **Behaviour** (`alarm_resolution`) | `alarm_notification` drops its own 6-minute check (`checkUnresolved`) and instead delivers whatever `escalateDue()` returns. |
| Acknowledging and resolving alarms, and merging repeat alarms | **Behaviour** (`alarm_resolution`) | `alarm_notification` calls `add()` when it raises an alarm and skips delivery if the answer is "joined an open alarm". |
| Who receives an alarm, and how it reaches their dashboard | **Software decisions** (`alarm_notification`) | It chooses the first destination role and passes it to `add()`. |
| Whether a manual inventory change is valid (no negative count, explanation required, low-stock alarm) | **Behaviour** (`inventory_record`) | `manual_inventory_changes` keeps the dashboard side: reading the request, turning +/- amounts into final counts, and replying to the user. It calls `InventoryRecord::manualChange` for each medication. |
| Counts per ADC: withdrawals, deposits, over-withdrawal discrepancies, and the per-ADC low-stock and discrepancy alarms | **Behaviour** (`inventory_record`) | The router calls `InventoryRecord::withdraw` and `InventoryRecord::deposit` instead of `InventoryStore::setCount`, and stops raising `DISCREPANCY` itself. |
| Hospital-wide stock: pharmacy reserves, totals across all ADCs, and the hospital low-stock alarm | **Behaviour** (`hospital_stock`, new) | Nothing to give up: `sw_decisions` has no reserves, no totals and no capacity. See section 3. |
| What the daily report and the activity summary contain, and when the daily report is due (06:00) | **Behaviour** (`report_types`) | `report_generation` stops choosing report contents. |
| How a report looks as text | **Behaviour** (`report_formatting`) | |
| Storing events permanently and reading them back | **Software decisions** (`report_generation` / audit trail) | Behaviour modules write and read through two small interfaces (section 6). |
| Who may do what | **Software decisions** (`user_settings`) | Behaviour modules contain no role rules. They ask `Permissions::mayPerform(role, action)`. |
| ADC registry, users and login, encryption, patient-data stripping, event parsing, message routing, startup | **Software decisions** | No overlap. Unchanged apart from the shared types below. |
| Missed heartbeats → Offline → alarm | **Behaviour** (`adc_heartbeats`) | The router already passes heartbeats to a `HeartbeatMonitor`; an adapter forwards them to `heartbeatReceived(adc)`. |
| The real clock and the ADC link | **Hardware hiding** (Matthew) | |

## 2. Shared types

These must be the same everywhere, or the two halves cannot call each other.

| Type | Proposal | Why |
|------|----------|-----|
| Time | Whole seconds (`long long`), hospital local time, no time zones. Text like `2026-11-02 06:00:00` is only for display and for reading dashboard input. | Subtracting seconds is simple and can't go wrong. Inodh's own time checks already convert text to seconds first (`timeutil::toSeconds`). |
| ADC id | Positive `int`. Shown to people as `ADC-<n>`. | Matches Inodh's registry. |
| "No ADC" / "whole hospital" | Behaviour uses **0**; Inodh uses **-1** (`Event::adcId` for an event not tied to an ADC, and `kAllAdcs` for an all-ADC window). | Either value works, but we should pick one. Until then, the adapters turn 0 into -1 and back. A hospital-wide alarm is shown as `Hospital`. |
| Alarm kind | Behaviour uses an `AlarmType` enum; Inodh's notifier takes a text code (`"TEMPERATURE"`, `"DISCREPANCY"`, …) and classifies it with `AlarmSeverityRules::classify`. | The adapter turns each `AlarmType` into a code, for example `HospitalLowStock` becomes `"HOSPITAL_LOW_STOCK"`, and `InventoryDiscrepancy` becomes Inodh's existing `"DISCREPANCY"`. Behaviour's `AlarmSeverity` table can then implement `classify`. |
| Role | One enum: `Pharmacist, Technician, Administrator`. | Inodh's `UserRole` and Andrew's `Role` have the same values. Keep one and delete the other. |
| History entry | One record type with: kind, who, detail, time, ADC, medication. Inodh's `fields` map can carry anything extra. | Both sides already write almost the same information. |
| Writing to history reports success or failure | `record()` returns `Stored` or `StorageFailure`, not `void`. | The rules say an alarm must not be marked resolved, and a count must not change, unless the record was stored. That's impossible to check if storing can't report failure. |
| Namespaces | Behaviour code is in `hms::behaviour`. Suggest `hms::sw` for software decisions and `hms::hw` for hardware hiding. | Without them, `AlarmSeverity` and `ReportTypes` clash today (checked by compiling both sides together). |

## 3. New behaviour module: Hospital stock

**What it does.** It keeps the hospital's active inventory of each medication:
the pharmacy reserves plus every ADC. It says when a medication is low across
the whole hospital, and raises a `HospitalLowStock` alarm (severity High) for
it. Pharmacists and administrators can also make manual changes to a reserve
count, with an explanation.

**What it hides.** How the hospital totals are calculated: the reserve count
and capacity plus the sum of every ADC's count and capacity, with the same 25%
rule as for one ADC applied to the totals. It also hides where reserves are
held: one count and capacity per medication, kept inside this module.

**Where it sits.** Under Inventory management, next to inventory record and
low stock rule. In `module_diagram.txt` that is
`behav_hiding/inventory_manager/hospital_stock.cpp`; please add it there,
since the tree file is outside Andrew's folder.

**Uses.** Inventory record (it only reads the ADC counts), low stock rule,
clock, history, the alarm notifier and permissions. Inventory record doesn't
use it, so there is no cycle.

**Requirements it traces to.**
- **FR-INV-01** (keep an active inventory): the hospital's active inventory,
  not only each ADC's.
- **FR-INV-02** (25% low-stock threshold): applied across all ADCs plus the
  reserves, as well as per ADC.

**How it differs from Inodh's spec.** Inodh's code only knows stock per ADC.
`InventoryStore` has a count per ADC and medication, with no capacity, no
reserves, no totals and no low-stock check. So Hospital stock duplicates
nothing in it.

The per-ADC rule stays where it is: one low ADC raises `InventoryLow` for that
ADC. The hospital view can disagree with it in both directions, and both cases
are tested:
- **One ADC low, hospital not low:** the reserves cover it, so only the ADC
  alarm is raised.
- **Every ADC fine, hospital low:** the reserves are nearly empty, so only the
  hospital alarm is raised.

**Permission choice.** A manual reserve change is the same kind of act as a
manual ADC change, so it reuses the existing `ManualInventoryChange`
permission (Pharmacist or Administrator) instead of adding a new one.

**When the alarm is raised.** Only when a medication goes from not low to
low. While it stays low, no more alarms are raised, even after the alarm is
resolved. It must recover, and a check must see that, before it can raise
again. The tick (or a check after each inventory change) does the checking.

**Alarm routing.** `HospitalLowStock` is High. Inodh's notifier already sends
High alarms to the pharmacist, which is the right person.

**Not built yet** (to agree first):
- Deliveries into the reserves.
- Transfers from the reserves into an ADC.

## 4. Permission table

Proposed final table. `user_settings` would hold it; the behaviour fake holds
the same table for tests.

| Action | Pharmacist | Technician | Administrator |
|--------|:---:|:---:|:---:|
| Schedule maintenance | | yes | |
| Cancel maintenance | | yes | |
| Manual inventory change (an ADC or a reserve) | yes | | yes |
| Acknowledge alarm | yes | yes | yes |
| Resolve alarm | yes | yes | yes |
| Generate activity summary | yes | | yes |

Where this differs from `user_settings.cpp` today (unchanged by the new
headers):

- **Schedule maintenance:** currently Administrator only. Proposed: Technician only.
- **Cancel maintenance:** no permission exists yet. Proposed: Technician only.
- **Manual inventory change:** currently Pharmacist only. Proposed: add
  Administrator. The same permission covers reserve changes.
- **Acknowledge alarm:** no permission exists yet. Proposed: all three roles.
- **Resolve alarm** and **Generate reports:** these already match.

The other permissions in `user_settings` (register, relocate and remove ADCs;
manage users; unlock accounts; view ADCs and inventory) are not used by
behaviour hiding and stay as they are.

## 5. What each of Inodh's files would need

| File | Changes |
|------|---------|
| *(all)* | Put the code in `namespace hms::sw`. Use seconds for time. |
| `external_interfaces.hpp` | Several interfaces are replaced by behaviour modules:<br>• `AlarmResolver` becomes `AlarmResolution`.<br>• `ReportTypes` becomes behaviour's `ReportTypes` plus `ReportFormatting`.<br>• `InventoryStore` becomes `InventoryRecord`.<br>`AlarmSeverityRules` and `HeartbeatMonitor` can stay as thin adapters (section 6). `Clock` changes to seconds (Matthew's). `CommsLink`, `DashboardOutput` and `AuditTrail` stay. |
| `adc_registry.cpp` | Nothing in the logic. An adapter (section 6) lets behaviour modules ask it whether an ADC is active and which ADCs are active. |
| `alarm_notification.cpp` | Remove the 6-minute `checkUnresolved`. `raise` calls `AlarmResolution::add` and skips delivery on "joined open alarm". Each tick, it delivers what `AlarmResolution::escalateDue()` returns. The `AlarmResolver` interface goes away, because `AlarmResolution::find(id)` gives the state. Its recipient chart (High goes to the pharmacist) already suits `InventoryLow` and `HospitalLowStock`. |
| `events_logs/event_contents.cpp`, `event_formatting.h` | No change in logic. If history entries become `Event`s, add a `ReserveChange` event type, or carry the kind in `fields`. |
| `events_logs/message_routing.cpp` | Withdrawals call `InventoryRecord::withdraw` and deposits call `InventoryRecord::deposit`, instead of `setCount`. Heartbeats go through the heartbeat adapter. Both sides now agree on over-withdrawal:<br>• The count goes to 0.<br>• It is recorded.<br>• A discrepancy alarm (High) is raised.<br>Inventory record now does all of that, so `updateInventory` and its own `DISCREPANCY` alarm go away; otherwise the alarm would be raised twice. A withdrawal or deposit of 0 or less is rejected by inventory record. `event_contents` lets a quantity of 0 through (it checks "non-negative"), so either it rejects 0 too, or the router treats an `IncorrectValue` from inventory record as a dropped message. |
| `maintenance_scheduler.cpp` | Retire it, or reduce it to turning dashboard text into a `Maintenance::schedule` / `cancel` call. Rules not yet carried over: **"all ADCs" windows** and **"start must not be in the past"**. Andrew's spec allows windows already under way, so we need to decide on both. |
| `manual_inventory_changes.cpp` | Keep the request handling and the dashboard replies. Add an explanation to each request; FR-INV-04 requires one. Replace the negative-count and storage logic with one `InventoryRecord::manualChange` call per medication. Drop the `InventoryStore` interface. Decide whether the 1,000,000 cap and the "zero change" rejection are wanted. Reserve changes from the dashboard call `HospitalStock::manualReserveChange` in the same way. |
| `message_processing/encryption.cpp` | No overlap. Open question below about exceptions. |
| `message_processing/patient_info_stripping.cpp` | No overlap. |
| `report_generation.cpp` | Keep event storage (`record`, `AuditTrail`). Change `record` to return success or failure. Implement the two history adapters (section 6). Report requests from the dashboard call `ReportTypes::summary` and `ReportFormatting::formatSummary` instead of its own `ReportTypes` interface (`isKnownType`, `includes`, `format`), which goes away. The daily report is started by `ReportTypes::generateDaily`, called from `startup/daily_tasks` on every tick. |
| `startup/authorization.cpp` | No change. Whether a request is allowed is still checked here as well. Both checks read the same table, so they cannot disagree. |
| `startup/initialize.cpp` | Create the behaviour modules (including `HospitalStock`) and the adapters, and pass them in. `tick()` must also call:<br>• `heartbeats.checkAll()`<br>• `hospitalStock.checkAll()`<br>• `alarms.escalateDue()`, and deliver the result<br>• `daily.tick()` (already there), which calls `reports.generateDaily(now)` through the `DailyReportGenerator` interface<br>Remove the old maintenance tick. |
| `user_information/user_registry.cpp` | No overlap. Consider not throwing from `getUserRole` for an unknown user (see below). |
| `user_information/user_settings.cpp` | Apply the table changes from section 4, and add a `Permissions` adapter. |

## 6. Adapters

An adapter is a small class that makes one side's existing code fit the other
side's interface, without changing either module. Each is a few lines. The
behaviour interfaces are in `Subgroup_B/behaviour_hiding/interfaces/`.

| Adapter | Implements (behaviour interface) | Wraps (Inodh's code) | What it does |
|---------|-----------------------------------|----------------------|--------------|
| Registry adapter | `AdcRegistry`: `isActive(adc)`, `activeAdcs()` | `ADClist` | `isActive` is `exists(id)`. `activeAdcs` lists the ids from `all()`. |
| History writer | `History`: `record(entry)` returns `Stored` or `StorageFailure` | `ReportGenerator::record` / `AuditTrail::append` | Copies each field into an `Event`: kind, who, detail and medication into `fields`; ADC into `adcId` (0 becomes -1); time as text. Returns `StorageFailure` if the store fails. |
| History reader | `HistoryReader`: `read(from, to)` | `AuditTrail::search(from, to)` | Turns the times into text, searches, and turns each `Event` back into an entry (-1 becomes 0). Returns a failure if the search fails. Note: `search` includes `to`; behaviour reads `[from, to)`. |
| Permissions adapter | `Permissions`: `mayPerform(role, action)` | `UserSettings::isAllowed(role, permission)` | Maps each `Action` to one of Inodh's `Permission` values, and `Role` to `UserRole` until we keep only one. |
| Alarm notifier | `AlarmNotifier`: `raise(adc, type, detail)` | `AlarmNotifier::raise(adcId, code, description)` | This is how ADC heartbeats, inventory record and hospital stock raise alarms. It turns the type into a code, passes the detail as the description, and turns 0 into -1. |
| Alarm severity rules | Inodh's `AlarmSeverityRules::classify(code)` | Behaviour's `AlarmSeverity::severityOf(type)` | This one goes the other way: it turns the code back into a type and returns the severity from behaviour's table. |
| Heartbeat monitor | Inodh's `HeartbeatMonitor::heartbeat(adcId, timestamp)` | `AdcHeartbeats::heartbeatReceived(adc)` | This one also goes the other way: the router's heartbeats reach ADC heartbeats. The timestamp isn't needed, because ADC heartbeats reads the clock. |
| Report sender | `ReportSender`: `send(dailyReport)` | `DashboardOutput::deliver`, optionally `EncryptionManager` | Turns the report into text with `ReportFormatting::formatDaily` and delivers it. Returns a failure if delivery fails. |
| Daily report trigger | Inodh's `DailyReportGenerator::generateDaily(now)` | `ReportTypes::generateDaily(now)` | Goes the other way: `daily_tasks` calls it every tick. Turns the text time into seconds and passes it on; report types decides whether the report is due. |
| Heartbeat counter | `HeartbeatCounter`: `count(adc, from, to)` | the router or history (to agree) | Counts the heartbeats received from one ADC in a time window, for the daily report. |
| Maintenance status | `MaintenanceStatus`: `isInMaintenance(adc, now)` | none | Already implemented by `Maintenance`. Nothing to write. |
| Clock | `Clock`: `now()` in seconds | Matthew's real clock | Matthew's clock implements this directly. Inodh's `timeutil` stays for turning seconds into text and back. |

## 7. Open questions

1. **"All ADCs" maintenance windows** (in Inodh's scheduler, not in Andrew's spec): keep them? If so, they go into `maintenance`.
2. **Maintenance start in the past:** Inodh rejects it; Andrew's spec allows a window already under way. Which wins?
3. **Alarm recipients:** Inodh sends Critical alarms to both Administrator and Technician. Alarm resolution holds one destination role per alarm. Is one role enough, or should it hold a list?
4. **Exceptions:** several of Inodh's functions throw:
   - encryption (bad key, bad or tampered packet)
   - `getUserRole` (unknown user)
   - `generateEncrypted`
   - startup checks

   Andrew's brief says "return a verdict, never throw, never halt the system". Is that rule for the whole team? Throwing at startup is reasonable. Throwing while running, for example on a tampered packet, could stop the monitoring system unless the router catches it.
5. **Escalation:** the spec says escalation creates a new alarm; `alarm_resolution` changes the existing one. Which wins?
6. **Offline alarm type:** ADC heartbeats raises `AdcOffline` (Critical), not `ConnectionLoss` (Low). Please confirm.
7. **Withdrawal larger than the count:** settled in favour of the router's behaviour. Inventory record now records it, sets the count to 0, and raises `InventoryDiscrepancy` (High). Inodh: is High the severity you intended for `DISCREPANCY`?
8. **Deposits:** settled. `InventoryRecord::deposit` adds to the ADC count (above capacity is allowed). Still open: should a deposit into an ADC also take the stock out of the pharmacy reserves (a transfer)? It doesn't today.
9. **"No ADC" value:** 0 (behaviour) or -1 (Inodh)? Either is fine; we should pick one.
10. **Moving the folder:** when `sw_decisions/` moves into `Subgroup_B`, use `git mv` so its history is kept, and agree a time so nobody is editing it during the move.
11. **Module tree:** please add `hospital_stock` under `behav_hiding/inventory_manager/` in `module_diagram.txt`.
