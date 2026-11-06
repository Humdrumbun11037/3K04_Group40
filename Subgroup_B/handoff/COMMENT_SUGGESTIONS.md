# COMMENT_SUGGESTIONS: plain-English comments for Inodh's and Matthew's code

What this is: a suggested one- or two-line comment for each major class and function in `sw_decisions/` (Inodh) and `Subgroup_B/hardware_hiding/` (Matthew). Andrew did not edit these files.
How to read it: one table per file. Paste the suggestion as `// ...` on the line above the declaration in the `.hpp` (or above the definition if you prefer). "Has one" means the function already has a comment that does the job; the suggestion is optional.
The style matches Andrew's Phase 2 comments: what it does and why it matters, as you'd explain it to a classmate. Edit freely; nothing here changes behaviour.
Written 2026-11-06 from the code as pulled that morning. Where a suggestion records a known issue, it says so in brackets; drop the bracket text if you'd rather keep that in OPEN_QUESTIONS.md.

## sw_decisions/ (Inodh)

### external_interfaces.hpp
| Item | Suggested comment |
|------|-------------------|
| `Clock` | The current time as text. Software decisions never reads the computer clock itself, so tests can use a fake time. |
| `timeutil::toSeconds` | Turns a "YYYY-MM-DD HH:MM:SS" time into seconds, so times can be compared and subtracted safely. |
| `CommsLink` | Where packets from the ADCs arrive; hides how the link to the cabinets works. |
| `DashboardOutput` | Puts a message on one user's dashboard; hides how the dashboard is shown. |
| `AuditTrail` | Permanent, searchable store of every event; hides files and encryption at rest. |
| `AlarmSeverityRules` | Decides how serious an alarm code is; behaviour hiding owns the table. |
| `AlarmResolver` | Tracks whether an alarm has been resolved, so notification knows when to stop chasing it. |
| `ReportTypes` | Decides what goes into each kind of report and how it is laid out. |
| `DailyReportGenerator` | Has one job: send the daily report when it is due. Called every tick. (Has one.) |
| `InventoryStore` | The medication counts per ADC that the router and manual changes update. |
| `HeartbeatMonitor` | Receives each ADC heartbeat so a silent ADC can be noticed. |

### adc_registry.hpp / .cpp
| Item | Suggested comment |
|------|-------------------|
| `class ADClist` | The list of ADCs the HMS knows about. Messages from any other ADC are ignored, so this is the HMS's guest list. |
| `addADC` | Registers a new ADC with a location; every change is logged so it can be audited. (Has one.) |
| `removeADC` | Takes an ADC off the network, for example when it is decommissioned. |
| `relocateADC` | Records that an ADC has moved, keeping the old location in the log. |
| `exists` / `find` / `all` | Look-ups used by the router and the dashboard. |
| `describe` | A plain sentence for each result, so users see why a request failed. |

### alarm_notification.hpp / .cpp
| Item | Suggested comment |
|------|-------------------|
| `class AlarmNotifier` | Gets each alarm to the right people. Severity decides who is told, so serious alarms reach someone who can act. |
| `raise` | Classifies a new alarm, starts tracking it and sends it to the roles responsible. Returns its id. |
| `checkUnresolved` | Run on every tick: re-sends alarms left unresolved for 6 minutes to the administrators. [Escalated alarms leave `pending()` even if unresolved; see OPEN_QUESTIONS 17.] |
| `pending` | Alarms still waiting for someone, for the dashboard. |
| `recipientsFor` | Who hears about each severity: Low to technicians, High to pharmacists, Critical to administrators and technicians. (Has one.) |
| `notifyRole` | Sends a message to everyone in a role; if nobody holds that role, administrators get it so the alarm is never lost. |
| `package` | Builds the one-line alarm message users see. |

### maintenance_scheduler.hpp / .cpp
| Item | Suggested comment |
|------|-------------------|
| `class MaintenanceWindow` | One booked maintenance period, which moves Pending to Active to Finished as time passes. |
| `MaintenanceWindow::update` | Moves the window on to its next state when its start or end time arrives, and logs the change. |
| `class MaintenanceScheduler` | Books and tracks maintenance windows, so planned downtime is not reported as a fault. |
| `scheduleMaintenance` | Checks a requested window (valid times, not in the past, at most 12 h, no overlap) and books it. |
| `cancel` | Removes a window that has not started yet. |
| `tick` | Run regularly: starts and finishes windows on time and clears finished ones. (Has one.) |
| `maintenanceActive` | Whether maintenance is under way, for one ADC or any. |
| `sanityCheck` | The booking rules in one place, checked in order; the first failure is the answer. |

### manual_inventory_changes.hpp / .cpp
| Item | Suggested comment |
|------|-------------------|
| `class ManualInventoryChanges` | Handles a user's request to correct stock counts by hand, and tells them whether it worked. |
| `apply` | Applies all the changes for one ADC or none of them, so a half-finished correction can never happen. (Has one.) [FR-INV-04 asks for an explanation; see OPEN_QUESTIONS 19.] |

### report_generation.hpp / .cpp
| Item | Suggested comment |
|------|-------------------|
| `class EventRecorder` | What every module talks to when something happens. Kept abstract so modules can be tested without storage. (Has one.) |
| `class ReportGenerator` | Logs every event to permanent storage and gathers stored events into reports. |
| `record` | Stores one event, stamping the time if the producer left it blank. (Has one.) |
| `generate` | Builds a report of one type for a time period, after checking the type and the dates. |
| `generateEncrypted` | The same report, encrypted so it can safely leave the HMS. |

### events_logs/
| Item | Suggested comment |
|------|-------------------|
| `event_formatting.h`: `eventRules` | The list of message keywords and the fields each one carries; adding a new kind of event means adding a row here. |
| `findRule` | Finds the rule for a keyword, or nothing if the word does not start an event. |
| `eventTypeName` / `eventTypeFromName` | Turn an event type into its stored name and back. |
| `class EventScanner` | Recognises events in an ADC message and logs them, so every withdrawal, deposit, alarm and login is on record. |
| `scan` | Finds the first event keyword, reads its fields, checks them and logs the event. (Has one.) |
| `fieldsAreValid` | Extra checks per event type, for example that a quantity is a whole number. |
| `class MessageRouter` | Decides what happens to each incoming ADC message and in what order, so patient data is removed before anything else sees it. |
| `routePacket` | Decrypts a packet, strips patient data, then routes it; a damaged packet is dropped, not fatal. |
| `routeMessage` | Drops unknown ADCs, passes heartbeats on, and logs events, raising alarms or updating stock as needed. |
| `pollLink` | Handles every packet waiting on the link. [Loops until the link says "nothing waiting"; see OPEN_QUESTIONS 14.] |
| `updateInventory` | Applies a withdrawal or deposit to the stock; a withdrawal larger than the count is still applied and flagged, because the ADC has already dispensed. |

### message_processing/
| Item | Suggested comment |
|------|-------------------|
| `class IEncryptionStrategy` | One encryption algorithm. Kept swappable so the algorithm can change without touching the rest of the HMS. (Has one.) |
| `class EncryptionManager` | The single place the HMS encrypts and decrypts, so the key is held in one spot. (Has one.) |
| `decryptADCMessage` | Opens a packet from an ADC; throws if it was tampered with or the key is wrong. |
| `encryptForReport` / `encryptForADC` | Lock data before it leaves the HMS. |
| `generateKey` | Makes a fresh random key. |
| `class PatientInfoStripper` | Removes patient-identifying fields from ADC messages, so no patient data is stored or shown by the HMS. |
| `strip` | Keeps every field except patient fields, in order, and wipes its working copy. |
| `isPatientField` | Spots a patient field by its `PT_` prefix, ignoring case and leading spaces. (Has one.) |

### startup/
| Item | Suggested comment |
|------|-------------------|
| `class Authorization` | Logs users in, keeps their sessions, and checks every action against their role. |
| `login` | Checks the password and handles locked accounts; gives the same answer for a wrong id or a wrong password so ids aren't revealed. |
| `liveSession` | Ends a session that timed out or whose account was removed or locked. (Has one.) |
| `authorize` | The yes/no check before any action; it also keeps the session alive. (Has one.) |
| `class DailyTasks` | The jobs that run once a day: the daily report and archiving old records. |
| `DailyTasks::tick` | Gives the daily report its chance every tick, and archives on the first tick of each day. (Has one.) |
| `loadOrCreateEncryptionKey` | Loads the saved key, or makes and saves one, so stored records can still be read after a restart. |
| `class HMSystem` | Builds every module in the right order and connects them; this is the running HMS. |
| `loadCheckedSettings` | Refuses to start with a role table that could lock everyone out. (Has one.) |
| `createInitialAdministrator` | Creates the very first administrator; refuses once one exists, so it can't be used to take over. (Has one.) |
| `HMSystem::tick` | One pass of the main loop: messages, alarms, maintenance and daily jobs. (Has one.) |

### user_information/
| Item | Suggested comment |
|------|-------------------|
| `class UserSettings` | The table of what each role may do; changing permissions means changing only this table. |
| `isAllowed` | Answers "may this role do this?". |
| `roleName` / `roleFromName` / `permissionName` | Names for showing to users and reading their input. |
| `class UserRegistry` | Stores user accounts and passwords safely, and locks an account after repeated failed logins. |
| `addUser` / `removeUser` | Create and delete accounts; a password is stored only as a salted hash. |
| `authenticate` | Checks a password, counts failures, and locks the account after 4 in a row. |
| `unlockAccount` | Lets an administrator reopen a locked account. |
| `changePassword` | Changes a password after checking the old one. |
| `getUserRole` | The user's role; throws for an unknown user. |
| `usersWithRole` | Everyone in a role, so alarms can be sent to them. |
| `hashPassword` | Turns a password into a hash so it is never stored as plain text. (Has one: placeholder to replace.) |

## Subgroup_B/hardware_hiding/ (Matthew)

| File | Item | Suggested comment |
|------|------|-------------------|
| clock/clock.hpp | `class SystemClock` | The real clock: reads the computer's local time, so nothing else has to. |
| clock/clock.cpp | `SystemClock::now` | The current local time as "YYYY-MM-DD HH:MM:SS". |
| communications_link/communications_link.hpp | `class HardwareCommsLink` | Connects the HMS to the ADC transceiver, hiding how the cabinets are reached. |
| communications_link/communications_link.cpp | `receive` | The next message from an ADC as bytes, or nothing if none is waiting. [The stub transceiver never reports "nothing"; see OPEN_QUESTIONS 14.] |
| dashboard_manager/dashboard_manager.hpp | `class ConsoleDashboardManager` | Shows dashboard messages on the console for now; a real screen can replace it without changing the rules. |
| dashboard_manager/dashboard_manager.cpp | `deliver` | Shows one message to one user. (Has one.) |
| use_Input_Handler/user_input_handler.hpp | `class UserInputHandler` | Where raw user input comes from; it doesn't decide what the input means. |
| use_Input_Handler/user_input_handler.hpp | `class ConsoleInputHandler` | Reads user input from the console. |
| use_Input_Handler/user_input_handler.cpp | `getInput` | Reads one line, or nothing when input has ended. |
| long_term_storage.hpp | `class FileAuditTrail` | Keeps every HMS event in encrypted files, so the history survives restarts and can't be read off the disk. |
| long_term_storage.cpp | `append` | Encrypts one event and adds it to the active file with the next sequence number. |
| long_term_storage.cpp | `search` | Every stored event in a time range, active and archived, oldest first. |
| long_term_storage.cpp | `archiveOlderThan13Months` | Moves records over 13 months old to the long-term file, copying first and only then removing, so nothing is lost if it fails part-way. |
| long_term_storage.cpp | `findNextSequenceNumber` | Finds where numbering left off after a restart, so every record keeps a unique number. |
| long_term_storage.cpp | `serializeRecord` / `deserializeRecord` | Turn an event into one line of text and back. |
| long_term_storage.cpp | `thirteenMonthsAgo` | The cut-off date for archiving, counted in calendar months. (Has one.) |
| long_term_storage.cpp | `replaceFile` | Swaps in a rewritten file safely, keeping a backup until the swap succeeds. |
