# Notes for the D3 integration report

A running list, updated as the integration is built. "P" = a problem hit,
with its root cause. "S" = what would have made the integration smoother.
"R" = a risk or near-miss.

## Problems and root causes

**P1. The ADC never sends anything to the HMS.**
- What happened: `HW1::Send_Message` is only ever called with `"Status Update"`, from `BH0::send_message()`, which nothing calls. Dispenses and alarms only reach the ADC's own console and event log.
- Root cause: neither side's D2 wrote the interface down as a message specification. Each subgroup's D2 described its own side in prose, so Subgroup A stopped at a printing stub.
- Fix: `integration/` reads the ADC's console output through one table of its print strings (ADC_HMS_INTERFACE.md, D1).

**P2. Matthew's HMS comms link would hang the HMS.**
- What happened: `HardwareCommsLink::receive()` wraps the ADC's `HW1::Receive_Message()`, which always returns `"HMS_ACK"`. `MessageRouter::pollLink` loops until `receive()` returns nothing, so it never returns. The bytes it passes on are also unencrypted, but the router expects encrypted packets.
- Root cause: there was no agreement on what "no message waiting" looks like or which side encrypts. One subgroup's hardware stub was reused as the other's transport.
- Fix: the comms link isn't used. Packets go to the router one at a time, as Subgroup B's integrated server already does.

**P3. The same things have different names on each side.**
- What happened: alarm codes differ (`INVALID_LOGIN_MAX` vs `UNAUTHORIZED_ACCESS`; `HARDWARE_JAM` and `TIMEOUT_FAULT` have no HMS type at all). So do roles (Nurse and Admin vs Technician, Pharmacist, Administrator), units (mg vs counts), capacity (none vs required) and ADC ids (none vs a positive int).
- Root cause: each subgroup built from its own D1 with no shared glossary.
- Fix: the mapping table; ADC-3 fixed; jam and timeout sent under their own codes. They stay unresolvable on the HMS (an honest mismatch).

**P4. The HMS design document and its code disagree.**
- What happened: HMS D2 §3.1 says the HMS requests a periodic message and that every message carries an increasing sequence number. The code has the ADC push heartbeats and has no sequence numbers.
- Root cause: the design was written before the message path was coded and was not re-checked against it.

**P5. The committed ADC program is a macOS build.**
- What happened: `Subgroup_A/adc_system` is a Mach-O arm64 file and doesn't run on the Windows demo laptop.
- Root cause: build output was committed, and the team works on mixed platforms.
- Fix: `integration/` builds everything from source, and its `.gitignore` excludes all binaries.

**P6. The HMS dashboard only refreshed on a click, and its clock was simulated.**
- What happened: it was built to drive a simulation, not to show a live feed, so a dispense on the ADC wouldn't appear until someone clicked.
- Fix: the integration launcher adds a 2 s auto-refresh when it serves the page, and runs the simulated clock in real time.

**P7. On the ADC, a Ping (even a successful one) silently stopped dispensing. FIXED.**
- What happened: `BH0::verify_connection` set Offline mode on both outcomes (`BH0_HMSComms.cpp:15`, with a comment saying an Online mode was never declared). `BH3::RequestDrug` then refused outside Dispensation mode with no message (`BH3_InventoryManager.cpp:35`). The screen said "OFFLINE (Local Dispensing Only)", but nothing dispensed until the user logged out and in again.
- Found by: planning the off-normal end-to-end test. Reproduced on Subgroup A's own standalone build.
- Root cause: Subgroup A's mode diagram has no "online" state, so the success branch reused Offline as a placeholder. The HMS stub always answered "OK", so the offline path had never run. And "Offline: local dispensing only" was never connected to `RequestDrug`'s mode check.
- Fix (approved by the team; the only change made in a subgroup's folder):
  - `Subgroup_A/src/BH0_HMSComms.cpp`: a successful Ping no longer sets Offline. An ADC that went Offline on an earlier failed Ping returns to Dispensation. A failed Ping still sets Offline and raises `CONNECTION_LOST` (beep and strobe).
  - `Subgroup_A/src/BH3_InventoryManager.cpp`: dispensing is also allowed in Offline mode, as the screen promises and NFR-SAF-01 requires.
- Tests: `tests/adc_ping_test.cpp` (5 tests, Subgroup A's real BH0 and BH3 against a stand-in HMS); the e2e Ping steps; the standalone diff in "Verification".

**P8. The dashboard needs page changes we can't make in Subgroup B's files.**
- What happened: cpp-httplib serves mounted folders before custom handlers, so adding the auto-refresh script meant `hms_live` serves the page's three files itself (the files on disk are unchanged).
- Components tab: Subgroup B's tab said the ADCs and link are SIMULATED, which is wrong for ADC-3.
- Fix: `hms_live` rewrites that one entry in `/api/state` as it serves it, into a REAL ADC-3 entry plus a simulated ADC-1/ADC-2 entry. `live_adc_test` pins Subgroup B's original text, so a change there fails the test instead of silently showing the old label.

**P9. On the ADC, a jammed drawer still counts as a dispense.**
- What happened: `BH3::RequestDrug` ignores `HW0::Open_Drawer`'s return value. With the simulated drawer in jam mode (`drawer jam`), the drawer never opens, yet the dispense is logged, the stock goes down and the HMS records a withdrawal.
- Found by: the swappable drawer stub (`adc_side/hw0_drawer_sim.cpp`). Pinned by `hw0_drawer_test`.
- Root cause: Subgroup A's drawer stub always returned success, so the failure branch was never written.
- Fix: not fixed (outside the approved change). It is a real FMEA row for the ADC: "actuator fails to open; stock and records say dispensed; patient not given the medication".
- Contrast: the stuck-open case does work end to end. Subgroup A's 30 s drawer timeout puts the ADC in Fault mode, and `TIMEOUT_FAULT` reaches the HMS.

**P10. A failed link looked like a working build.**
- What happened: after a new dependency (`stampOf`) was missed in `hms_live`'s link line, the link failed. On this MSYS2 setup the g++ driver printed only "ld returned 1/5 exit status", with no reason. Make then kept the half-written `hms_live.exe`, so the next run failed with "Exec format error" and every HMS check in the e2e test failed.
- Root cause: our makefile had no `.DELETE_ON_ERROR`, and the real error only showed when `ld` was run directly.
- Fix: `.DELETE_ON_ERROR` is now in the makefile, and the link line is fixed.
- Lesson: the e2e test caught it immediately, because it checks the running programs, not just the build.

**P11. Code that runs before `main()` touched Subgroup A's data too early.**
- What happened: the first version of the cabinet selection ran before `main()`, so the ADC appears as its chosen cabinet from the start. It loaded the cabinet's stock into Subgroup A's SW1 stock map, a static in another file that C++ may not have constructed yet. The program died silently right after claiming the cabinet. The HMS showed the cabinet as taken while the ADC window stayed blank.
- Root cause: C++ leaves the order in which different files' statics are set up undefined, and the integration had to start before Subgroup A's unchanged `main()`.
- Fix: before `main()` only the console hooks are put in (they use nothing of Subgroup A's). The selection runs at Subgroup A's first output, when `main()` is running but before its first menu appears.

**P12. Two timelines in one test drifted apart.**
- What happened: the e2e test drives the ADC with fixed sleeps while a second script checks the HMS. Each check starts curl and Python, about a third of a second on Windows, so by the Swap ADC stage the checks ran seconds after the ADC had already swapped back.
- Fix: the ADC script now waits for a flag file the checker writes, instead of a fixed sleep.
- Also: lines printed by Subgroup A's drawer-safety thread sat in stdout's buffer when output went to a file, and were lost when the test stopped the ADC. The console observer now flushes after each line.

**P13. Restocks from the ADC's menu never reached the HMS (our bug, found late).**
- What happened: Subgroup A prints its prompts without a newline (`Amount to Add: `), and the restock's log line follows on the same console line. The output table expected the log text at the start of a line, so every restock typed at the menu was ignored.
- Why it went unnoticed: dispenses worked by luck, because a drawer line ends the prompt line first. The restock test called `BH3::addDrug` directly, with no prompt before it, so it passed. The README had already claimed "Restock → HMS deposit" as integrated.
- Found by: trying the README's own demo step by hand (restock on ADC-1).
- Fix: rows now match wherever their text starts on the line (still anchored at its end). There is a table test with the real prompt-then-log line, and an e2e restock from the menu.
- Lesson: test through the real user path (the menu) and not only by calling functions; and walk through the demo script by hand before trusting it.

## Risks and near-misses

**R1. Output observation depends on the ADC's exact print strings.**
- Mitigation: one table, plus a test that runs Subgroup A's real code and pins its source strings.
- Checked by mutation: changing `" dispensed "` in a copy of BH3 makes the test fail.

**R2. Advancing the simulated clock while the real ADC is connected would wrongly mark it Offline.**
- Mitigation: the launcher refuses Advance while the link is live.

**R3. Events queued while the link was down are timestamped when they arrive, not when they happened. Mitigated.**
- Root cause: the ADC has no clock (it logs `[TIMESTAMP]`), and the message format has no time field.
- Mitigation: the link now stamps every message with the real local time at the moment the ADC printed it (`|ADC_TIME=...`, a trailing field Inodh's parser ignores).
- `hms_live` writes a message more than 5 s late into the feed, and writes an `AdcLateMessage` history entry carrying its real time.
- Still a limit: Inodh's own history entry keeps the arrival time.

**R4. The HMS and the ADC each keep their own counts, so they can drift apart.**
- How: the ADC restarts at Tylenol 100 while the HMS keeps running, or the ADC restocks a medication the HMS doesn't stock at ADC-3 (the HMS refuses it as `UnknownItem`).
- Root cause: the interface has no message that carries the full stock.

**R5. Two threads printing at once could split a line.**
- How: the observer passes each line on whole, but Subgroup A prints one line in several writes (`<< text << "\n"`). BH3's drawer-safety thread prints "Drawer N safely secured" while the menu prints.
- Why the effect is small: a dispense is logged before that thread starts, so only unused lines could be split. Line splitting itself is tested with whole-line writes.

**R6. Heartbeats are faster than the HMS needs.**
- The ADC heartbeats every 20 s against the HMS's 60 s period, so a single lost heartbeat never takes it Offline.
- Offline needs 180 s of silence. The demo shortens the wait with Advance, which is allowed only while the link is cut.

**R8. One cabinet program can be any cabinet (demo only).**
- How it works: Subgroup A's ADC has no ADC id. For the demo it asks which registered ADC it is, at start and on "Swap ADC", and loads that cabinet's stock from the HMS. The HMS treats a picked cabinet as real (no simulated heartbeats) until the program swaps away.
- Limits: Subgroup A keeps one event log and one user list per program, shared by every cabinet it is swapped to. A swap is refused while messages are still queued, so nothing is sent under the wrong id.
- A race found and removed: at first any authentic packet made its ADC real again. A heartbeat already on its way during a swap then took back the cabinet just released. Now only an explicit claim makes an ADC real.

**R7. The HMS also raises IncorrectMessageFormat now (not done by Subgroup B's router).**
- What happened: HMS D2 §3.1 requires a Critical `IncorrectMessageFormat` alarm for an unreadable message; Inodh's router only drops it.
- Fix: `hms_live` raises it when the router rejects a packet (`Malformed`, `NotAnEvent`, or an exception). It goes to the sending ADC if the packet decrypts, else to ADC 0, which the dashboard shows as "Hospital".
- Repeats join the open alarm.
- Unknown ADCs are still only dropped, as D2 says.

## Verification so far

**Automated:**
- `make test`: 56 tests pass (output table 12, HMS side 22, ADC link 8, Ping 5, cabinet selection 6, drawer 3).
- `make e2e`: 33 checks pass across the cabinet-selection, normal, cut, restore, Swap ADC and stuck-drawer stages (4 runs out of 4).
- `make regression`: Subgroup A's standalone build (with the Ping fix) and self-test pass; so do Subgroup B's 9 behaviour programs, 24 API tests and 42 integrated tests.

**Console output:**
- Subgroup A standalone, before vs after the Ping fix: identical for a session without a Ping. With a Ping, the only differences are the state line after the Ping (DISPENSATION MODE instead of OFFLINE) and the dispense after it now happening, which also lowers the later counts.
- Integrated ADC vs standalone (drawer mode normal, HMS up): identical except for the integration's own lines (the selection screen, `This screen: ADC-n` under the title, `[4] Swap ADC` under the login menu).

**Drift checks:** changing `" dispensed "` in a copy of Subgroup A's BH3 makes the output-table test fail. Changing Subgroup B's Components text makes `live_adc_test` fail.

**Files changed outside `integration/`:** only the approved Ping fix, `Subgroup_A/src/BH0_HMSComms.cpp` and `BH3_InventoryManager.cpp` (checked by modification time).

## What would have made it smoother

**S1.** One written, versioned ADC ↔ HMS interface document, agreed by both subgroups during D2. It should list every message, field, code, unit and id, plus the behaviour when either side is down.

**S2.** Shared code for the message format and encryption that both sides compile, instead of each side inventing its own end.

**S3.** A stub HMS for Subgroup A and a stub ADC for Subgroup B, from D2 on, so each side tests against the other's interface early.

**S4.** Hardware stubs that can be set to fail (not "always OK"), so failure paths like a dropped link get exercised before integration.

**S5.** No committed binaries, and one build command checked on every platform the team uses.

**S6.** Use every hardware return value. The jam (P9) and the Ping (P7) defects both hid behind stubs whose failure branch never ran; one test per failure branch would have found them in D2.
