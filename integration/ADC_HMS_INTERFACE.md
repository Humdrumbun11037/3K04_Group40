# ADC ↔ HMS interface (D3 integration)

The agreed interface between Subgroup A's Automated Dispensing Cabinet (ADC)
and Subgroup B's Hospital Monitoring System (HMS), as built in `integration/`.
All the integration code is new and lives in `integration/`. The only edit to a
subgroup's code is the team-approved Ping fix in Subgroup A (REPORT_NOTES P7),
which isn't needed for the link itself. Section 6 lists the decisions and where
this differs from each side's D2.

## 1. Direction and roles

| Direction | What crosses | When |
|-----------|--------------|------|
| ADC → HMS | Heartbeat | every 20 s while the link is up (the HMS period is 60 s) |
| ADC → HMS | Withdrawal (a dispense) | as soon as the ADC logs it |
| ADC → HMS | Deposit (a restock) | as soon as the ADC logs it |
| ADC → HMS | Alarm | as soon as the ADC raises it |
| HMS → ADC | `HMS_ACK` reply to each packet; reply to a ping | on each request |
| HMS → ADC (demo only) | The list of registered ADCs; the chosen cabinet's stock | when the cabinet program starts, and on "Swap ADC" (section 7) |

The HMS never sends a command that affects dispensing, so the HMS being down,
restarting or cut off can never stop the ADC dispensing (NFR-SAF-01).

## 2. Transport

- HTTP/1.1 over TCP, using cpp-httplib (`Subgroup_B/third_party/httplib.h`).
- The HMS listens on **127.0.0.1:9040** (one laptop). The dashboard stays on 127.0.0.1:8081.
- `POST /adc/packet`, body = one encrypted packet (`application/octet-stream`).
  The reply is `200` with body `HMS_ACK <router outcome>`, or `503` when the link is cut for the demo.
- `GET /adc/ping` replies `200 HMS_ACK`. The ADC's "Ping HMS Network" (menu option 5) uses it.
- Timeouts: 1 s to connect, 2 s to read. Every network call on the ADC is made by a background thread,
  except the ping, which a user asks for explicitly.

## 3. Packet

`[12-byte nonce][ciphertext][16-byte tag]`: ChaCha20-Poly1305 from Inodh's
`EncryptionManager` (`sw_decisions/message_processing/encryption.*`), with the
pre-shared 32-byte key in `integration/run/data/key.bin`. The HMS creates the
key on its first start, and the ADC only reads it.

## 4. Message (plaintext inside the packet)

Inodh's existing format (`sw_decisions/events_logs/event_formatting.h`):
`<adcId>|<keyword>|<field>|...`. Fields never contain `|` (it is replaced by `/`).
`<adcId>` is the ADC the cabinet program was told it is when it started, or
after a "Swap ADC" (section 7). It can be any registered ADC; `hms_live`
registers **ADC-3** (Ward 5) for it at start, next to the simulated ADC-1 and ADC-2.
The examples below use ADC-3.

Every message except a heartbeat ends with `|ADC_TIME=YYYY-MM-DD HH:MM:SS`: the
ADC computer's local time when the ADC printed the line. Inodh's parser reads
only the fields its rule names, so it ignores this field. `hms_live` uses it to
note, and record in history, messages that arrive more than 5 s late (decision D8).

| Message | Fields | Made from (ADC output table row) |
|---------|--------|-----------------------------------|
| `3\|Heartbeat` | none | the link itself |
| `3\|Withdrawal\|<med>\|<qty>\|<user>` | qty in mg, sent as a plain count (1 mg = 1 unit) | R1 `<user> dispensed <n>mg of <med>` |
| `3\|Deposit\|<med>\|<qty>\|<user>` | as above | R2 `<user> restocked <n>mg of <med>` |
| `3\|Alarm\|UNAUTHORIZED_ACCESS\|<text>` | | R3 security relay (3 failed logins, or SECURITY_BREACH) |
| `3\|Alarm\|TIMEOUT_FAULT\|<text>` | **no HMS alarm type** (D3) | R4 drawer left open > 30 s |
| `3\|Alarm\|HARDWARE_JAM\|<text>` | **no HMS alarm type** (D3) | R5 menu option 3; R6 self-test |
| `3\|Alarm\|CONNECTION_LOSS\|<text>` | | the ADC's ping fails (sent once the link is back); R7 self-test |

All string matching lives in `adc_side/adc_output_table.cpp`.
`tests/adc_output_table_test.cpp` runs Subgroup A's real code, and pins
Subgroup A's source strings, so a change to any of them fails the test.

**What the HMS does with each message** (Subgroup B code, unchanged):

| Message | HMS behaviour |
|---------|---------------|
| Heartbeat | Missed count reset, status Online (`AdcHeartbeats`) |
| Withdrawal / Deposit | Logged; count changed by inventory record. A withdrawal larger than the count sets it to 0 and raises DISCREPANCY (High). A count below 25 % of capacity raises InventoryLow (High). An item not stocked at ADC-3 is refused and shown in the feed |
| Alarm with a known code | Delivered to the role's dashboards (Inodh's chart) and tracked by alarm resolution, where it can be acknowledged and resolved |
| Alarm with an unknown code (TIMEOUT_FAULT, HARDWARE_JAM) | Classified **Critical**, delivered to Administrator and Technician dashboards and logged, but **cannot be acknowledged or resolved** on the HMS (alarm resolution has no type for it) |
| Bad key, tampered packet, or bad format | Dropped by the router (`Malformed` / `NotAnEvent`); the HMS keeps running. `hms_live` raises **IncorrectMessageFormat** (Critical), as HMS D2 §3.1 requires: for the sending ADC if the packet decrypts, else for ADC 0 ("Hospital"). Repeats join the open alarm |
| Unregistered ADC id | Dropped (`UnknownAdc`) |

## 5. When the other side is unavailable

| Situation | ADC (Subgroup A + link) | HMS |
|-----------|-------------------------|-----|
| HMS unreachable or link cut | Keeps dispensing. Messages wait in a queue of up to 200 (oldest dropped first, and counted). Heartbeats are not queued. The console prints one `[HMS-LINK]` line when the link goes down and one when it comes back. Ping fails → ADC goes OFFLINE (local dispensing only) and beeps (`CONNECTION_LOST`, Subgroup A's own behaviour) and `CONNECTION_LOSS` is queued; it still dispenses | After 3 missed 60 s periods: ADC-3 Offline, `AdcOffline` (Critical) raised once |
| Link restored | Queued messages are sent in order, then heartbeats resume. Ping succeeds → back to Dispensation mode | ADC-3 back Online (history entry); queued events arrive. Inodh's history stamps them on arrival; the feed and an `AdcLateMessage` history entry give their real ADC time. Open alarms stay until resolved |
| Link up | Ping succeeds; the mode does not change (fixed in Subgroup A, REPORT_NOTES P7) | |
| HMS restarted | as "unreachable", then "restored" | Fresh audit trail and fresh counts (ADC-3 seeded again with Subgroup A's starting stock) |
| ADC stopped | n/a | as "link cut" |

## 6. Decisions and known differences

- **D1 Output observation, not code changes.** Subgroup A never sends anything to the HMS: `HW1_Transceiver` only prints, and `BH0::send_message` is never called. The integrated ADC is built from Subgroup A's files, with its two hardware stubs swapped at build time:
  - `HW1_Transceiver.cpp` → `adc_side/hw1_transceiver_net.cpp`: same functions and console output, plus the link;
  - `HW0_DrawerController.cpp` → `adc_side/hw0_drawer_sim.cpp`: same output in normal mode, plus stuck and jam modes for the hardware-fault demo.

  The link reads the ADC's console output through the output table. The only edit to Subgroup A's code is the team-approved Ping fix (REPORT_NOTES P7).
- **D2 The ADC pushes.** HMS D2 §3.1 says the HMS requests a periodic message; the HMS code instead waits for heartbeats. The ADC pushes.
- **D3 Jam and drawer timeout are sent under their own codes.** This is an honest mismatch: the HMS shows them as Critical but cannot resolve them.
- **D4 Which ADC (demo cabinet selection).** Subgroup A has no ADC id. The cabinet program asks which registered ADC it is, at start and on "Swap ADC" (section 7), and loads that cabinet's stock from the HMS. A cabinet the HMS holds no stock for gets Subgroup A's starting stock (Tylenol 100, Morphine 50) with capacities 200 and 100 (Subgroup A has no capacity), plus a Tylenol pharmacy reserve of 300 out of 400. In the real world each cabinet is one ADC with its own screen, so the selection is marked demo only.
- **D5 Units.** mg on the ADC, unitless counts on the HMS: 1 mg = 1 unit.
- **D6 Not sent:** logins (the ADC never prints the user name), the ADC's mode, and remaining counts (the HMS keeps its own).
- **D7 Not implemented:** sequence numbers (in HMS D2, but not in either side's code), and the HMS → ADC status request.
- **D8 Time.** Inodh's code stamps events on arrival, using the HMS's simulated clock, which runs in real time from 2026-11-02 05:40. The ADC has no clock (it logs `[TIMESTAMP]`), so the link stamps each message with the ADC computer's real local time (`ADC_TIME`) when the line is printed. Advance is refused while the real ADC is connected, because it would otherwise go Offline even though it is sending.

## 7. Demo cabinet selection (HMS → ADC, demo only)

A real cabinet is one ADC with its own screen and never changes. For the demo,
one cabinet program can be any registered ADC, so a dispense or restock goes
into the cabinet you picked, and a second window (`run_adc.bat`) can be a second
cabinet. Both requests are encrypted with the shared key, and so are their answers.

| Request (ADC → HMS) | Answer (HMS → ADC) |
|---------------------|--------------------|
| `POST /adc/cabinets`, body `List` | One line per registered ADC: `<id>\|<location>\|real` or `...\|simulated`. ADCs registered on the dashboard during the demo are included |
| `POST /adc/claim`, body `<id>\|Claim\|<previous id or 0>` | `OK\|<location>\|<med>=<count>;...` (that cabinet's counts on the HMS), or `ERR\|<reason>` |

**On the ADC (Subgroup A's code unchanged):**
- Before Subgroup A's first menu, a "SELECT ADC (demo only)" screen lists the cabinets.
  `ADC_ID=<n>` skips it, for scripts.
- Subgroup A's stock (SW1) is set to the claimed cabinet's counts through SW1's own functions.
  A medication the cabinet doesn't hold is set to 0, and a new one gets the next free drawer.
- Subgroup A's screens get two extra lines:
  - `This screen: ADC-<n>, <location> (demo)` under the title;
  - ` [4] Swap ADC (demo only)` under the login menu.
- `4` typed at the login menu is taken before Subgroup A's `main()` reads it.
- A swap is refused while the HMS is unreachable or messages are still waiting to be sent, so nothing is sent under the wrong id.

**On the HMS:**
- A claimed ADC is **real**: the simulator stops sending its heartbeats, so a dropped link shows. `/api/resume` and `/api/heartbeat` are refused for it.
- The previous ADC goes back to the simulator.
- Real ADCs stay real across an HMS Restart.
- Registering an ADC is an Administrator permission in Inodh's settings.
