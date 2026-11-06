# Hardware Hiding modules: module notes

Hardware-hiding modules of the Hospital Monitoring System (HMS), Subgroup B.

Each module hides a hardware-facing or implementation-specific decision so that Software Decisions and Behaviour Hiding depend on stable interfaces rather than details such as the operating system clock, physical communication mechanism, console input, dashboard implementation, or file format.

## Notation and conventions

- Hardware Hiding provides concrete implementations of interfaces used by the higher-level HMS modules.
- The interface definitions are owned by the relevant subsystem or teammate. Hardware Hiding implements those interfaces; it should not redefine their responsibilities.
- The modules should hide implementation decisions that may change independently of HMS requirements.
- The HMS is centralized. Hardware Hiding's communications module provides a link between the HMS and ADC communication interface; it does not implement the ADC's dispensing logic.
- Storage records are encrypted before being written to persistent files. Encryption is provided by Software Decisions through `EncryptionManager`.
- Hardware Hiding implementation errors must be handled according to the interface contract. The storage implementation currently uses C++ exceptions for invalid data and file-operation failures; this differs from Behaviour Hiding's rejection-verdict convention.

## Module overview

| # | Module | Files | Class / implementation | Responsibility |
|---|---|---|---|---|
| 1 | Clock | `hardware_hiding/clock/clock.hpp/.cpp` | `SystemClock` | Provide the current time without exposing the operating-system clock implementation. |
| 2 | Communications link | `hardware_hiding/communications_link/communications_link.hpp/.cpp` | Communications link implementation | Hide the ADC transceiver API and communication details from Software Decisions. |
| 3 | Dashboard manager | `hardware_hiding/dashboard_manager/dashboard_manager.hpp/.cpp` | Dashboard manager implementation | Hide how HMS information and notifications are presented to the user dashboard. |
| 4 | User input handler | `hardware_hiding/use_Input_Handler/user_input_handler.hpp/.cpp` | `ConsoleInputHandler` | Hide how raw user input is obtained from the console or another input source. |
| 5 | Long-term storage | `hardware_hiding/long_term_storage.hpp/.cpp` | `FileAuditTrail` | Persist HMS audit events in encrypted files, retrieve them, and archive old records. |

---

## 1. Clock

**Responsibility**

Provide a consistent source of current time to the HMS without exposing operating-system-specific time functions.

**Secret**

How the current time is obtained from the computer's system clock and converted into the representation expected by the rest of the HMS.

**Implementation**

`SystemClock` is the concrete clock implementation in:

- `hardware_hiding/clock/clock.hpp`
- `hardware_hiding/clock/clock.cpp`

**Known interface behavior**

The implementation provides `SystemClock::now()`, returning a string representation of the current time in the version previously compiled during integration.

**Dependencies**

Uses the computer's system clock. It should not depend on Behaviour Hiding business rules.

**Assumptions and design considerations**

- Higher-level modules should obtain time through the Clock interface rather than calling operating-system time functions directly.
- The representation returned must agree with the interface expected by its consumer.
- Time-zone and timestamp-format decisions must be consistent across the HMS.
- The Behaviour Hiding specification describes time as seconds since the Unix epoch in hospital-local time. The Software Decisions integration has also used string timestamps, so the final interface contract and conversion responsibility must be agreed between the groups.

**Effects**

Reads the current system time and returns it to the caller. It does not change HMS state.

**Exceptions / failure behavior**

The exact failure behavior should be documented from the final implementation and agreed interface. No special rejection-verdict table has been established for this module.

---

## 2. Communications link

**Responsibility**

Provide a communication interface between the centralized HMS and ADCs without exposing the underlying transceiver implementation.

**Secret**

The choice and operation of the underlying communication mechanism, including the transceiver functions used to receive messages, send responses, and check the connection.

**Implementation**

Files:

- `hardware_hiding/communications_link/communications_link.hpp`
- `hardware_hiding/communications_link/communications_link.cpp`

The implementation uses the Subgroup A transceiver interface declared in:

- `Subgroup_A/inc/HW1_Transceiver.hpp`

The relevant functions are in namespace `HW1`:

- `HW1::Receive_Message()`
- `HW1::Send_Message(std::string)`
- `HW1::Check_Connection()`

The corresponding implementation is in `Subgroup_A/src/HW1_Transceiver.cpp`.

**Dependencies**

- Subgroup A transceiver interface.
- Software Decisions communication-link interface.

**Assumptions and design considerations**

- Software Decisions should use the HMS communication interface rather than call `HW1` functions directly.
- The communication link transports messages; validation, interpretation, event detection, and alarm decisions belong to Software Decisions.
- A communication failure must not directly affect an ADC's ability to dispense medication.
- The current Subgroup A transceiver implementation is a stub: it prints diagnostic messages, returns `"HMS_ACK"` for receiving, and returns `1` for sending and connection checking. This is not yet evidence of working physical ADC communication.

**Effects**

Receives incoming communication data and provides the ability to send responses and check the communication connection through the transceiver interface.

**Exceptions / failure behavior**

The exact return-value mapping and failure behavior of the Hardware Hiding wrapper should be checked against its final header and implementation. The underlying transceiver currently uses return values rather than a demonstrated physical communication error protocol.

---

## 3. Dashboard manager

**Responsibility**

Hide the implementation details of presenting HMS information to users through a dashboard.

**Secret**

How dashboard information is formatted, displayed, or otherwise delivered to the user-facing dashboard.

**Implementation**

Files:

- `hardware_hiding/dashboard_manager/dashboard_manager.hpp`
- `hardware_hiding/dashboard_manager/dashboard_manager.cpp`

**Dependencies**

- Software Decisions dashboard-output interface.
- The output mechanism used by the implementation.

**Assumptions and design considerations**

- Higher-level HMS modules should communicate with the dashboard through an abstract dashboard interface.
- Alarm and status decisions belong to Software Decisions or Behaviour Hiding, not to the dashboard implementation.
- The dashboard manager is responsible for presenting information, not deciding whether a user is authorized to perform an operation.
- The implementation should be replaceable without changing the business rules of the HMS.

**Effects**

Delivers dashboard information through the concrete output mechanism.

**Exceptions / failure behavior**

The exact methods, formatting behavior, and output-failure handling should be taken from the final header and implementation. No separate business-rule verdict table has been defined for this module.

---

## 4. User input handler

**Responsibility**

Obtain raw user input without making decisions about what the input means or whether it is valid.

**Secret**

How input is collected from the console or another input source.

**Implementation**

Files:

- `hardware_hiding/use_Input_Handler/user_input_handler.hpp`
- `hardware_hiding/use_Input_Handler/user_input_handler.cpp`

The implementation includes `ConsoleInputHandler`.

**Dependencies**

- Software Decisions user-input interface.
- Console input mechanism.

**Assumptions and design considerations**

- The input handler provides raw input only.
- It does not validate commands, interpret user intent, apply permissions, or execute HMS operations.
- Software Decisions is responsible for interpreting and validating the input.
- This separation allows the console input mechanism to be replaced without changing the HMS's business logic.

**Effects**

Reads input and returns the raw input to the caller.

**Exceptions / failure behavior**

The previously integrated implementation returns a raw `UserInput` value through an optional result. The exact handling of empty input, end-of-input, and console failures should be confirmed against the final interface.

---

## 5. Long-term storage

**Responsibility**

Provide persistent storage for HMS audit events while hiding file-system operations, record serialization, encryption at rest, searching, and archival.

**Secret**

- The file format used to store events.
- How events are serialized and deserialized.
- How records are encrypted before being written to disk.
- How encrypted records are read and decrypted.
- How records are assigned sequence numbers.
- How duplicate records are handled during searching and archival.
- How old records are moved from active storage to long-term storage.

**Implementation**

Files:

- `hardware_hiding/long_term_storage.hpp`
- `hardware_hiding/long_term_storage.cpp`

Concrete class:

`FileAuditTrail`

Constructor:

`FileAuditTrail(const std::string& activeFilename, const std::string& longTermFilename, EncryptionManager& encryption);`

Public operations:

- `append(const Event& event)`
- `search(const std::string& from, const std::string& to) const`
- `archiveOlderThan13Months(const std::string& now)`

**Dependencies**

- Software Decisions `AuditTrail` interface.
- Software Decisions `Event` data type.
- Software Decisions `EncryptionManager`.
- C++ file-system and file-stream facilities.

**Assumptions and design considerations**

- The active audit file and long-term archive file are supplied to the constructor.
- A shared `EncryptionManager` is supplied by the application composition layer.
- The storage module does not independently generate or own the master encryption key.
- Events are serialized, encrypted, converted to hexadecimal text, and then written to the storage file.
- When records are read, the hexadecimal representation is decoded and decrypted before deserialization.
- Sequence numbers support record identification and deduplication.
- Archival moves records older than the 13-month retention threshold from active storage to the archive.
- Archived records remain encrypted.
- Searching considers both active and archived records and returns the matching events in chronological order.
- The storage implementation validates timestamps and record data rather than silently accepting malformed records.

**Effects**

- `append` persists an encrypted event.
- `search` reads and decrypts matching records without changing the logical event history.
- `archiveOlderThan13Months` moves eligible records to long-term storage while preserving their encrypted representation.

**Exceptions / failure behavior**

Unlike the Behaviour Hiding modules' rejection-verdict convention, the current storage implementation uses exceptions for failures such as:

- Invalid timestamps or time ranges.
- Invalid or malformed stored records.
- Encryption or decryption failures.
- File creation, opening, reading, or writing failures.

The caller must handle these failures appropriately. In particular, a storage failure must not be treated as a successful audit write.

---

## 6. Encryption key storage and application startup integration

**Important ownership distinction**

Encryption key persistence is currently implemented in Software Decisions, not in Hardware Hiding.

Files:

- `sw_decisions/startup/encryption_key_storage.hpp`
- `sw_decisions/startup/encryption_key_storage.cpp`

Function:

`loadOrCreateEncryptionKey(const std::string& filename)`

Its responsibilities are to:

- Load an existing key from disk.
- Generate a new key when one does not exist.
- Persist the generated key.
- Verify that the stored key has the expected `EncryptionManager::kKeySize`.
- Reject truncated or oversized key files.
- Create the parent directory when necessary.

`FileAuditTrail` receives an `EncryptionManager&`; it does not load the key itself.

**Required application composition**

The intended startup sequence is:

1. Load or create the encryption key.
2. Construct one `EncryptionManager` using that key.
3. Construct `FileAuditTrail` with a reference to that manager.
4. Construct `HMSystem` with a reference to that same manager.
5. Keep the manager alive for as long as either dependent object uses it.

This is important because `HMSystem` and `FileAuditTrail` share the same manager and do not own it.

**Security considerations**

- The current key-storage helper writes the key as raw binary.
- The real key file must not be committed to Git.
- The key file needs appropriate operating-system access restrictions.
- The key should be protected separately from the encrypted audit records.
- Encrypting audit records does not, by itself, solve secure key management.

**Current integration status**

The encrypted storage test passed. It verified that an event could be written, that the stored file did not expose the event's plaintext fields, and that a newly constructed encryption manager could reload the persisted key and decrypt the event after a simulated restart.

The full HMS application composition and startup wiring are still separate integration work.

---

## Integration and dependency overview

| Hardware Hiding module | Higher-level interface / consumer | Important dependency |
|---|---|---|
| Clock | Clock interface used by HMS modules | Operating-system time |
| Communications link | `CommsLink` | `HW1_Transceiver.hpp` |
| Dashboard manager | `DashboardOutput` | Dashboard output mechanism |
| User input handler | User input interface | Console input |
| Long-term storage | `AuditTrail` | `Event`, `EncryptionManager`, file system |

The intended dependency direction is:

- Software Decisions and Behaviour Hiding depend on abstract interfaces.
- Hardware Hiding provides concrete implementations of those interfaces.
- Hardware-specific and operating-system-specific details remain inside Hardware Hiding.
- The application composition layer connects the concrete implementations to the higher-level modules.

## Verified implementation and testing status

The following Hardware Hiding source files were integrated into the encryption worktree:

- `clock/clock.cpp`
- `communications_link/communications_link.cpp`
- `dashboard_manager/dashboard_manager.cpp`
- `long_term_storage.cpp`
- `use_Input_Handler/user_input_handler.cpp`

All five Hardware Hiding `.cpp` files passed C++17 syntax-only compilation with `-Wall -Wextra`.

The encrypted storage integration test also passed:

1. A key was created and persisted.
2. An event was written through `FileAuditTrail`.
3. The raw storage record did not contain the event's plaintext fields.
4. The first encryption manager was destroyed.
5. A second manager loaded the persisted key.
6. The stored event was successfully decrypted and recovered with its original fields.

The current Makefile compiles Behaviour Hiding and Hardware Hiding objects and the Subgroup A transceiver object. This is not yet a complete linked and running HMS application.

## Items to confirm with the team

1. Final interface signatures and return-value/error contracts for Clock, CommsLink, DashboardOutput, and UserInputHandler.
2. Whether the HMS clock interface should expose formatted strings or Unix-epoch seconds, and which layer performs conversion.
3. The final physical communication implementation and its failure behavior.
4. The dashboard output format and whether it is console-based or connected to another display mechanism.
5. The final production locations and access permissions for the active audit file, archive file, and encryption key.
6. The central application composition/startup code that constructs the concrete Hardware Hiding implementations and shares the encryption manager.
7. Whether storage exceptions are caught and translated at the appropriate higher-level boundary.

These notes describe the Hardware Hiding implementation integrated into Subgroup B and distinguish tested behavior from interfaces and startup work that still require agreement.