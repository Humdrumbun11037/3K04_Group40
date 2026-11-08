# Integration: ADC (Subgroup A) + Monitoring System (Subgroup B)

This folder joins Subgroup A's Automated Dispensing Cabinet to Subgroup B's
Hospital Monitoring System for D3. Every integration piece is new code here.
The subgroups' files are compiled as they are, into `integration/build/`.
The one exception is the team-approved Ping fix in Subgroup A (two lines; see
REPORT_NOTES P7).

The interface is in [ADC_HMS_INTERFACE.md](ADC_HMS_INTERFACE.md), and the problems
found along the way are in [REPORT_NOTES.md](REPORT_NOTES.md).

Toolchain: MSYS2 UCRT64 g++ (C++17) and `mingw32-make`. The end-to-end test also needs `curl` and `python`.

## Commands (from `integration/`)

| Command | What it does |
|---------|--------------|
| `mingw32-make` | Builds the three programs and the tests (first build takes about a minute) |
| `mingw32-make test` | 56 tests: output table 12, HMS side 22, ADC link 8, Ping 5, cabinet selection 6, drawer 3. The drawer test waits for Subgroup A's 30 s timeout, so this takes about 50 s |
| `mingw32-make e2e` | Both real programs on a timeline: cabinet selection, normal path, Ping, link cut, restore, Swap ADC, stuck drawer (33 checks, about 80 s) |
| `mingw32-make regression` | Both parts built standalone as their owners build them; Subgroup A's self-test and all of Subgroup B's tests |
| `run_demo.bat` | Opens the HMS window, a cabinet window and the dashboard |
| `run_adc.bat` | Opens one more cabinet window (a second cabinet) |
| `drawer.bat stuck` / `jam` / `normal` | Sets the simulated drawer hardware of the running ADC |
| `mingw32-make clean` | Deletes `build/` |

## Each part on its own

- **ADC:** `cd Subgroup_A && make && ./adc_system`. The committed `adc_system` is a macOS build, so rebuild it on Windows.
- **HMS:** `cd Subgroup_B/gui && mingw32-make run-integrated`, then open http://127.0.0.1:8081. It uses simulated ADC-1 and ADC-2.

Stop `run-integrated` before starting `hms_live`: both use port 8081.

## The integrated system

| Program | What it is |
|---------|------------|
| `build/hms_live` | Subgroup B's integrated HMS (unchanged code). ADC-1 (Ward 3 West) and ADC-2 (ICU) are Subgroup B's; ADC-3 (Ward 5) is added for the cabinet program. Each ADC is simulated until a cabinet window picks it. Dashboard on 127.0.0.1:8081; ADC endpoint on 127.0.0.1:9040. The clock runs in real time. Its window takes `cut`, `restore`, `status` and `quit` |
| `build/adc_hms_link` | Subgroup A's ADC (its code and menu) with its two hardware stubs swapped at build time: `HW1_Transceiver.cpp` → `adc_side/hw1_transceiver_net.cpp` (the link to the HMS), and `HW0_DrawerController.cpp` → `adc_side/hw0_drawer_sim.cpp` (drawers that can stick or jam). At start it asks which registered ADC it is (**demo only**: a real cabinet has its own screen) and loads that cabinet's stock from the HMS; `[4] Swap ADC` on the login menu changes it |
| `build/adc_stub` | A stand-in ADC: tests, and the **demo backup** (below) |

The HMS creates the shared key `run/data/key.bin` on its first start, so start it before the ADC.
For two laptops, run `hms_live --listen-all`, copy `key.bin` to the ADC laptop, and set
`HMS_HOST=<HMS laptop's IP>` there (single-laptop is the tested setup).

## Demo script (one laptop, about 10 minutes)

**Accounts.**
- ADC: `nurse_jane/hash123` (Nurse), `pharm_bob/hash456` (Pharmacist), `tech_tom/hash789` (Technician), `admin_alice/hash000` (Admin).
- HMS: the same names and passwords, without the Nurse (the HMS has no Nurse role): `pharm_bob/hash456` (Pharmacist), `tech_tom/hash789` (Technician), `admin_alice/hash000` (Administrator). `hms_live` puts these in place of Subgroup B's own demo accounts (tara, priya, alex), which still work in Subgroup B's standalone server.

**ADC menus.**
- Logged out: `1` log in, `2` self-test, `3` shut down, `4` Swap ADC (demo only).
- Logged in: `1` dispense, `2` restock, `5` Ping HMS, `8` clear faults (Technician), `9` log out, `10` shut down.

0. **Each part alone.** Show both, as in the section above. Close them.
1. **Start.**
   - Run `run_demo.bat`. The cabinet window shows **SELECT ADC (demo only)** with every registered ADC.
   - Type `3`: the window becomes ADC-3 (Ward 5) and loads its stock from the HMS. Explain that a real cabinet has its own screen; picking is only for the demo.
   - On the dashboard, log in as **pharm_bob** / `hash456`. The Components tab shows **Real cabinets: ADC-3**.
   - The ADC window says `[HMS-LINK] Connected to the HMS`, and its menu shows `This screen: ADC-3, Ward 5 (demo)`.
2. **Dispense crosses the boundary.**
   - On the ADC: `1`, then `nurse_jane` / `hash123`, `1`, `Tylenol`, `60`.
   - Within about 2 s the dashboard shows:
     - ADC-3 Tylenol 100 → 40 (low);
     - a feed line `Live ADC link: "3|Withdrawal|Tylenol|60|nurse_jane|ADC_TIME=..." -> Event`;
     - a **High InventoryLow** alarm in pharm_bob's inbox.
   - Resolve it with a note.
3. **Ping while the link is up.**
   - On the ADC: `5`. The Ping succeeds and the ADC stays in **DISPENSATION MODE**.
   - `1`, `Tylenol`, `5`: dispensing still works, and the dashboard shows Tylenol 35.
4. **ADC alarm crosses the boundary.**
   - On the ADC: `9` (log out), then 3 logins with a wrong password. The ADC locks, with security relay, beep and strobe.
   - The dashboard shows a **Critical UnauthorizedAccess** for ADC-3, delivered to admin_alice and tech_tom.
   - On the ADC, `tech_tom` logs in, `8` clears it, `9` logs out. On the HMS, log in as admin_alice and resolve the alarm.
5. **Off-normal: the link drops.**
   - Type `cut` in the HMS window.
   - On the ADC, log in as nurse_jane and dispense 5 Morphine. **It still dispenses.** The ADC window says `HMS not reachable ... messages are queued`.
   - Now `5` (Ping): **the Ping fails**. The ADC goes **OFFLINE (Local Dispensing Only)** and beeps (`CONNECTION_LOST`).
   - Dispense 2 Morphine: it still works offline.
   - On the dashboard, Morphine at ADC-3 is still 50. Press **Advance +3 min**, which is allowed now that the link is cut. ADC-3 goes **Offline** and a **Critical AdcOffline** alarm is raised.
6. **Link restored.**
   - Type `restore`. Within about 2 s the queued withdrawals arrive (Morphine 43).
   - The feed notes they arrived late, with the ADC's real time.
   - A Low **ConnectionLoss** arrives, and ADC-3 is Online again. AdcOffline stays open until someone resolves it.
   - On the ADC: `5`. The Ping succeeds and the ADC is back in DISPENSATION MODE.
7. **Swap ADC (demo only): stock goes into the cabinet you are at.**
   - On the ADC: `9` (log out), then `4` (Swap ADC). Pick `1`: the window becomes ADC-1 (Ward 3 West) and loads ADC-1's stock from the HMS (Insulin 30, Morphine 40).
   - Log in as `pharm_bob` / `hash456` and restock (`2`) 20 Morphine. On the dashboard, ADC-1's Morphine goes 40 → 60; ADC-3 is unchanged.
   - The Components tab now shows **Real cabinets: ADC-1**, and ADC-3 is simulated again.
   - `9`, `4`, `3` swaps back: ADC-3's counts are loaded from the HMS.
   - Optional: as admin_alice, register a new ADC on the dashboard (e.g. 5, Emergency). Run `run_adc.bat` for a second cabinet window: ADC-5 is in its list, and picking it gives it Subgroup A's starting stock.
8. **Hardware fault: a drawer sticks open.**
   - In a spare terminal: `drawer stuck`. On the ADC, dispense 5 Tylenol: the drawer opens and doesn't close.
   - After Subgroup A's 30 s drawer timeout, the ADC beeps and locks: **FAULT MODE**, "SYSTEM LOCKED: DRAWER TIMEOUT".
   - The dashboard gets a **Critical TIMEOUT_FAULT**, delivered to admin_alice and tech_tom. It is not resolvable on the HMS, because the HMS has no type for it (an honest mismatch).
   - Then `drawer normal`; on the ADC, `9`, then `tech_tom` logs in and `8` clears the fault.
9. **Optional.**
   - `drawer jam`, then dispense: the drawer never opens, yet the ADC records the dispense (REPORT_NOTES P9, an FMEA row). Then `drawer normal`.
   - `build/adc_stub --wrong-key "3|Heartbeat"`: the HMS drops it and raises a **Critical IncorrectMessageFormat**.

**Advance while the cabinet is connected.** The HMS refuses it, because the cabinet would miss heartbeats it never had the chance to send. **Restart and the scenario buttons** rebuild the HMS; ADC-3 is registered again with fresh stock, and the cabinets the windows drive stay real. The cabinet windows keep their own counts until they swap or restart, so they won't match the HMS's fresh counts.

### Demo backup (if the ADC program won't start)

Start `build/hms_live`. Then, in another window:

```
build/adc_stub --claim 3 --heartbeats 20   # takes ADC-3 over and keeps it Online (leave running)
build/adc_stub "3|Withdrawal|Tylenol|60|nurse_jane"
build/adc_stub "3|Alarm|UNAUTHORIZED_ACCESS|ADC security lockout"
build/adc_stub "3|Alarm|TIMEOUT_FAULT|Drawer 1 left open"
```

These are the same encrypted packets the real cabinet sends. If you use the stub, say so to the TAs.

## What is and is not integrated (for the TAs)

| Integrated, real on both sides | Not integrated, or a known gap |
|--------------------------------|--------------------------------|
| Dispense → HMS withdrawal, count, history, low-stock alarm | Logins: the ADC never prints the user name |
| Restock → HMS deposit | Medications the HMS doesn't stock at ADC-3 (refused, shown in the feed) |
| Lockout or security breach → UnauthorizedAccess (resolvable) | HARDWARE_JAM and TIMEOUT_FAULT: Critical and delivered, but **cannot be resolved** on the HMS (no HMS alarm type) |
| Heartbeats; link loss → Offline + AdcOffline; recovery; Ping reflects the real link | Sequence numbers and the HMS → ADC status request (in HMS D2, not in either side's code) |
| ChaCha20-Poly1305 encryption with a shared key (Inodh's code on both ends); unreadable packets → IncorrectMessageFormat | Inodh's history keeps the arrival time; the ADC's real time is in the feed and an `AdcLateMessage` entry |
| ADC keeps dispensing with the HMS down (also after a failed Ping); queued messages sent on reconnect | HMS clock is simulated (real-time, from 2026-11-02 05:40) |
| Drawer stuck open → Subgroup A's timeout → Fault mode → TIMEOUT_FAULT on the HMS | A jammed drawer is not detected by Subgroup A (P9) |
| Cabinet selection (demo only): any registered ADC, including one added during the demo; its stock comes from the HMS | Subgroup A keeps one event log and one user list for the program, shared by every cabinet it is swapped to |

## Files

| File | Role |
|------|------|
| `adc_side/adc_output_table.*` | The only place the ADC's print strings are relied on (rows R1–R7); the `ADC_TIME` stamp |
| `adc_side/console_observer.*` | Copies the ADC's console output to the table without changing it |
| `adc_side/adc_link.*` | Queue, background sender, heartbeats, retry |
| `adc_side/hms_client.*` | HTTP + encryption to the HMS endpoint |
| `adc_side/hw1_transceiver_net.cpp`, `hw1_net.hpp`, `hw1_autostart.cpp` | Build-time replacement for Subgroup A's `HW1_Transceiver.cpp`; puts in the console hooks before `main()`, and runs the cabinet selection and starts the link at Subgroup A's first output |
| `adc_side/cabinet.*` | Demo cabinet selection: the screen, the HMS's answers, loading a cabinet's stock into Subgroup A's SW1 |
| `adc_side/console_input.*` | Takes the `4` (Swap ADC) typed at the login menu before Subgroup A's `main()` reads it |
| `adc_side/hw0_drawer_sim.cpp` | Build-time replacement for Subgroup A's `HW0_DrawerController.cpp`: normal, stuck and jam modes |
| `hms_side/live_adc.*` | ADC-3 setup, the standard accounts, the cabinet list and claims, packets into Inodh's router, the IncorrectMessageFormat alarm, late-message notes, the Advance guard, the Components and login-hint corrections |
| `hms_side/hms_live_main.cpp`, `hms_side/web/live.js` | HMS launcher; dashboard auto-refresh |
| `tools/adc_stub.cpp` | Stand-in ADC |
| `run_demo.bat`, `run_adc.bat`, `drawer.bat` | Demo launchers |
| `tests/` | `adc_output_table_test`, `live_adc_test`, `adc_link_test`, `adc_ping_test`, `cabinet_test`, `hw0_drawer_test`, `fake_hms.hpp`, `e2e.sh` |
