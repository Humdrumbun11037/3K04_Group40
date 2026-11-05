# sw_decisions implementation progress

Worked top-down through `module_diagram.txt`, `sw_decisions` only.

**Up to:** all of `sw_decisions` is done (last module: `report_generation`).
**Next:** `hw_hiding/` (starting with `long_term_storage/audit_trail`), then `behav_hiding/`.

| # | Module | Status |
|---|--------|--------|
| 1 | events_logs/event_formatting.h | done |
| 2 | events_logs/event_contents | done |
| 3 | events_logs/message_routing | done |
| 4 | message_processing/encryption | done (ChaCha20-Poly1305, checked against RFC 8439 test vector; AES-256-GCM if built with `-DHMS_HAVE_OPENSSL -lcrypto`) |
| 5 | message_processing/patient_info_stripping | done |
| 6 | startup/authorization | done |
| 7 | startup/initialize | done |
| 8 | user_information/user_registry | done |
| 9 | user_information/user_settings | done |
| 10 | adc_registry | done |
| 11 | alarm_notification | done |
| 12 | maintenance_scheduler | done |
| 13 | manual_inventory_changes | done |
| 14 | report_generation | done |

## What hw_hiding / behav_hiding must provide

`sw_decisions/external_interfaces.hpp` lists every service `sw_decisions`
uses from the other two folders, as abstract classes. Each one maps to a
module in the diagram:

| Interface | Implement in |
|-----------|--------------|
| `Clock` | hw_hiding/clock |
| `CommsLink` | hw_hiding/comms_link |
| `DashboardOutput` | hw_hiding/dashboard_manager |
| `AuditTrail` | hw_hiding/long_term_storage (audit_trail + records-searching) |
| `AlarmSeverityRules` | behav_hiding/alarms/alarm_severity |
| `AlarmResolver` | behav_hiding/alarms/alarm_resolution |
| `ReportTypes` | behav_hiding/reports (report_types + report_formatting) |
| `InventoryStore` | behav_hiding/inventory_manager/inventory_record |
| `HeartbeatMonitor` | behav_hiding/messaging_period/adc_heartbeats |

`HMSystem` (startup/initialize) takes all of them in an `ExternalModules`
struct and connects everything. Call `HMSystem::tick()` from the main loop.

## Decisions made that the group should confirm

- **ADC message format:** `<adcId>|<keyword>|<fields...>`, e.g.
  `12|Withdrawal|Morphine|5|U1001`. Patient data is in fields starting
  `PT_` (e.g. `PT_NAME=...`) and is removed before anything else sees it.
  Heartbeats are `12|Heartbeat`. Field lists per keyword are in
  `event_formatting.h`.
- **Timestamps:** `YYYY-MM-DD HH:MM:SS` everywhere.
- **Role permissions:** see `user_settings.cpp` (e.g. only pharmacists do
  manual inventory changes; only administrators schedule maintenance).
- **Messages from unregistered ADCs** are dropped.
- **Withdrawal larger than the HMS count:** count goes to 0 and a
  `DISCREPANCY` alarm is raised.
- **Alarm with no one in the target role** goes to administrators instead.
- **Escalation:** unresolved after 6 min, sent to administrators once.
- **Maintenance windows:** at most 12 h, no overlaps per ADC, not in the past.
- **Sessions** time out after 15 min of no activity.
- **Password hashing** in `user_registry.cpp` is a placeholder (salted,
  iterated FNV-1a); swap in Argon2/bcrypt before real use.
