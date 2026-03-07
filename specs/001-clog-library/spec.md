# Feature Specification: clog Logging Library

**Feature Branch**: `001-clog-library`
**Created**: 2026-02-28
**Status**: Draft
**Input**: Existing spec.md — minimal C89 logging library for Classic Macintosh and modern systems

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Basic Log Lifecycle (Priority: P1)

A developer integrates clog into their application. They
initialize logging with an app name and severity level, write
log messages at four severity levels (error, warn, info,
debug), and shut down when the application exits. Messages
appear with a timestamp and level tag in the output.

**Why this priority**: This is the core value proposition.
Without init/write/shutdown working, nothing else matters.
Every downstream consumer depends on this lifecycle.

**Independent Test**: Can be fully tested by calling init,
writing one message at each level, verifying formatted output
appears, and calling shutdown. Delivers working log output on
a modern system.

**Acceptance Scenarios**:

1. **Given** a freshly linked application, **When** the
   developer calls init with an app name and INFO level, logs
   a message at INFO, and calls shutdown, **Then** the output
   contains a line matching `[<ms>][INF] <message>` followed
   by a newline.
2. **Given** logging initialized at WARN level, **When** the
   developer logs at DEBUG and INFO levels, **Then** no output
   is produced for those messages.
3. **Given** logging initialized at DEBUG level, **When** the
   developer logs at all four levels, **Then** all four
   messages appear in the output in order.
4. **Given** an initialized logger, **When** shutdown is
   called twice, **Then** no error or crash occurs.

---

### User Story 2 - Classic Mac File Output (Priority: P2)

A developer building a Classic Mac application uses clog to
write log messages to a text file on disk. The file is created
automatically on init, uses Mac line endings, and each write
is flushed immediately (Mac apps may crash, so every line must
be persisted).

**Why this priority**: Classic Mac support is the
distinguishing feature of clog. Classic Mac projects target
68k and PPC, so file-based logging on these platforms is
essential.

**Independent Test**: Can be tested by initializing clog on a
Mac target, writing several messages, then verifying the
resulting text file contains all expected lines with correct
timestamps and Mac line endings.

**Acceptance Scenarios**:

1. **Given** a Classic Mac application, **When** init is
   called with app name "MyApp", **Then** a text file named
   "MyApp" is created (or truncated if it exists) in the
   application's working directory.
2. **Given** an initialized Mac logger, **When** a message is
   written, **Then** the file grows by one line immediately
   (no buffering).
3. **Given** an initialized Mac logger, **When** shutdown is
   called, **Then** the file is closed cleanly and remains
   readable.
4. **Given** an initialized Mac logger, **When** a message
   is written, **Then** the line uses Mac-style line endings.

---

### User Story 3 - Compile-Time Stripping (Priority: P3)

A developer preparing a release build wants to remove all
logging with zero runtime overhead. By defining a single
compile-time flag, all log macros expand to nothing. A
separate flag allows keeping only high-severity messages
(error and warn) while stripping verbose ones.

**Why this priority**: Release builds for resource-constrained
Classic Mac hardware must not carry dead logging code. This is
the mechanism that makes clog practical for shipping products.

**Independent Test**: Can be tested by compiling a program
with the strip flag defined, then verifying (via binary
inspection or symbol listing) that no logging calls or strings
remain in the output.

**Acceptance Scenarios**:

1. **Given** a program using all four log macros, **When**
   compiled with the full-strip flag, **Then** no log function
   calls or format strings appear in the compiled output.
2. **Given** a program using all four log macros, **When**
   compiled with minimum level set to WARN, **Then** only
   ERROR and WARN log calls remain; INFO and DEBUG are
   eliminated.
3. **Given** a stripped build, **When** the application runs,
   **Then** no logging output is produced and no logging-
   related work is performed at runtime.

---

### User Story 4 - Custom Output Destination (Priority: P4)

A developer wants to redirect log output to a specific file
instead of the default destination (stderr on modern systems,
auto-named file on Mac). They specify the filename before
initialization, and all subsequent log output goes to that
file.

**Why this priority**: Useful for test harnesses and
situations where stderr is unavailable, but not required for
the core use case.

**Independent Test**: Can be tested by setting a custom
filename, initializing, writing messages, and verifying the
custom file contains the expected output.

**Acceptance Scenarios**:

1. **Given** a custom filename is set before init, **When**
   init is called and messages are logged, **Then** all output
   goes to the specified file.
2. **Given** no custom filename is set, **When** init is
   called on a modern system, **Then** output goes to the
   default destination (stderr).
3. **Given** a custom filename was set, **When** the
   developer passes NULL as the filename, **Then** the output
   reverts to the platform default.
4. **Given** init has already been called, **When** a custom
   filename is set after init, **Then** the setting has no
   effect on current output.

---

### Edge Cases

- What happens when the format string produces a message
  exceeding the buffer size? (Silently truncated.)
- What happens when clog_init fails (cannot create file)?
  (Returns error code; subsequent log calls are no-ops.)
- What happens when shutdown is called without a prior init?
  (No crash; safe to call in any state.)
- What happens when a message is logged before init is called?
  (No output produced; no crash.)
- What happens on Mac when disk is full during a write?
  (Write fails silently; application continues.)
- What happens when tv_usec wraps across a second boundary in
  the POSIX timestamp delta? (Borrow is handled correctly;
  no unsigned overflow.)
- What happens when a consumer compiles with -pedantic?
  (clog.h suppresses variadic macro warnings internally.)
- What happens when clog_set_file() is called after init?
  (Returns -1; caller can detect the error.)
- What happens on Classic Mac when heap is not expanded before
  init? (clog_init calls MaxApplZone internally; safe.)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Library MUST provide exactly four severity
  levels: error, warn, info, debug (in descending severity).
- **FR-002**: Library MUST provide an init function accepting
  an application name (truncated to 31 characters) and a
  minimum severity level, returning success or failure.
- **FR-003**: Library MUST provide a shutdown function that
  closes resources and is safe to call multiple times.
- **FR-004**: Library MUST filter messages below the
  configured minimum level (messages below threshold produce
  no output and minimal overhead).
- **FR-005**: Library MUST format each log line as
  `[<timestamp>][<LEVEL>] <message>\n` where timestamp is
  milliseconds since init and level is a 3-character
  abbreviation (ERR, WRN, INF, DBG).
- **FR-006**: Library MUST provide four convenience macros
  (one per level) that expand to the underlying write call.
- **FR-007**: Library MUST support compile-time stripping
  where all macros expand to no-ops when a strip flag is
  defined.
- **FR-008**: Library MUST support a compile-time minimum
  level flag that eliminates macros below the specified level.
- **FR-009**: Library MUST provide a function to override the
  default output destination with a named file, effective only
  when called before init.
- **FR-010**: Library MUST use only a fixed-size static buffer
  for message formatting (no dynamic memory allocation).
- **FR-011**: Library MUST be compatible with the C89/C90
  language standard.
- **FR-012**: On modern systems, library MUST write to stderr
  by default.
- **FR-013**: On Classic Mac, library MUST write to a text
  file using the platform file manager, with immediate flush
  on each write.
- **FR-014**: Library MUST provide a utility function that
  returns the human-readable name for a given severity level.
- **FR-015**: Library MUST build as a static library for each
  supported platform (modern POSIX, 68k Mac, PPC Mac).
- **FR-016**: Library MUST compute POSIX timestamps correctly
  across second boundaries without unsigned overflow.
- **FR-017**: Library header MUST suppress variadic macro warnings
  internally so consumers using -pedantic are unaffected.
- **FR-018**: `clog_set_file()` MUST return an error indicator
  (0 success, -1 if already initialized) instead of void.
- **FR-019**: Library MUST support CMake `add_subdirectory()` usage
  by guarding test targets behind an option flag.
- **FR-020**: `clog_write()` declaration MUST include a format
  attribute (when compiler supports it) for printf validation.
- **FR-021**: Classic Mac `clog_init()` MUST call `MaxApplZone()`
  to ensure heap is expanded before File Manager operations.
- **FR-022**: Library MUST provide an append mode option
  (`clog_set_append`) that preserves existing log file contents
  instead of truncating.
- **FR-023**: Library documentation MUST state the static memory
  footprint for each platform.

### Key Entities

- **Log Level**: One of four severity tiers (error, warn,
  info, debug) that controls which messages are output. Used
  at init time to set the threshold and at write time to tag
  each message.
- **Log Message**: A formatted string with timestamp, level
  tag, and user-provided content. Constrained to a fixed
  maximum size per platform (256 bytes modern, 192 bytes Mac).
- **Output Destination**: Where log messages are written.
  Defaults to stderr (modern) or an auto-named file (Mac).
  Can be overridden to a named file before initialization.

### Assumptions

- The library is single-threaded. No concurrent access from
  multiple threads or interrupt handlers.
- One logger instance per application. No support for multiple
  simultaneous loggers.
- The primary consumers are Classic Mac C projects that need
  cross-platform logging.
- Application name provided to init is a valid, non-NULL
  string.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can add logging to their
  application by including one header and linking one static
  library, with no additional configuration required.
- **SC-002**: Any C89 project successfully links against and
  uses the library on all three target platforms (POSIX, 68k
  Mac, PPC Mac) without modification.
- **SC-003**: A release build with stripping enabled contains
  zero logging function calls or format strings in the
  compiled output.
- **SC-004**: The entire library (header plus both platform
  implementations) totals under 500 lines of source code.
- **SC-005**: Each log message is formatted and written in
  under 1 millisecond on the target platform.
- **SC-006**: A new developer can understand the complete
  public interface (6 functions, 4 macros) within 5 minutes
  of reading the header.

## Feedback Log

- **2026-03-07**: POSIX timestamp overflow on second boundary → R1, T014
- **2026-03-07**: Variadic macro -pedantic incompatibility → R2, T015
- **2026-03-07**: clog_set_file() silent failure after init → R3, T016
- **2026-03-07**: CMake add_subdirectory() support → R4, T017
- **2026-03-07**: Format string -Wformat-security false positive → R5, T018
- **2026-03-07**: Classic Mac MaxApplZone() crash on init → R6, T019
- **2026-03-07**: Log file append mode → R7, T020
- **2026-03-07**: Memory footprint documentation → R8, T021
