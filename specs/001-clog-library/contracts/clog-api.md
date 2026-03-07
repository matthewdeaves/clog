# API Contract: clog Public Interface

**Phase**: 1 — Design & Contracts
**Date**: 2026-02-28

## Overview

clog exposes its entire public API through a single header
(`clog.h`). The contract consists of 1 enum type, 6 functions,
4 convenience macros, and 2 compile-time control flags.

## Types

### ClogLevel

```
Enumeration with 4 values:
  CLOG_ERR   = 1   (highest severity)
  CLOG_WARN  = 2
  CLOG_INFO  = 3
  CLOG_DEBUG = 4   (lowest severity)
```

## Functions

### clog_init

```
Signature: int clog_init(const char *app_name, ClogLevel level)
Purpose:   Initialize the logger
Input:     app_name — application identifier (truncated to 31 chars)
           level    — minimum severity level for output
Output:    0 on success, -1 on failure
Side effects:
  - Stores app name and level
  - Records start timestamp
  - Opens output destination (stderr or file)
  - On Mac: creates/truncates log file
Preconditions: None (safe to call in any state)
Postconditions: Logger is ACTIVE; writes are enabled
```

### clog_shutdown

```
Signature: void clog_shutdown(void)
Purpose:   Close resources and deactivate logger
Input:     None
Output:    None
Side effects:
  - Closes file handle (if open, not stderr)
  - Sets logger to UNINITIALIZED state
Preconditions: None (safe to call without prior init)
Postconditions: Logger is UNINITIALIZED; writes become no-ops
Idempotent: Yes (multiple calls are safe)
```

### clog_set_file

```
Signature: int clog_set_file(const char *filename)
Purpose:   Override the default output destination
Input:     filename — path to output file, or NULL for default
Output:    0 on success, -1 if already initialized
Side effects:
  - Stores filename for use by subsequent clog_init()
Preconditions: Must be called BEFORE clog_init()
Postconditions: Next init will use this file as output
Note: If called after init, returns -1 with no effect
```

### clog_set_append

```
Signature: int clog_set_append(int enable)
Purpose:   Enable append mode (preserve existing log file)
Input:     enable — 1 to append, 0 to truncate (default)
Output:    0 on success, -1 if already initialized
Side effects:
  - Sets append flag for use by subsequent clog_init()
Preconditions: Must be called BEFORE clog_init()
Note: If called after init, returns -1 with no effect
```

### clog_write

```
Signature: void clog_write(ClogLevel level, const char *fmt, ...)
           __attribute__((format(printf, 2, 3)))  /* GCC/Clang only */
Purpose:   Format and write a log message
Input:     level — severity of this message
           fmt   — printf-style format string
           ...   — format arguments
Output:    None (writes to output destination)
Side effects:
  - Formats message into static buffer
  - Writes to output if level <= configured minimum
Preconditions: Logger should be ACTIVE (no-op if not)
Behavior:
  - If level > configured minimum: return immediately
  - Format: [<ms_since_init>][<LVL>] <message><newline>
  - Truncates silently if message exceeds buffer
```

### clog_level_name

```
Signature: const char *clog_level_name(ClogLevel level)
Purpose:   Return human-readable name for a level
Input:     level — a ClogLevel value
Output:    Static string: "ERR", "WRN", "INF", "DBG",
           or "???" for invalid values
Side effects: None
```

## Macros

### Convenience Logging Macros

```
CLOG_ERR(fmt, ...)   → clog_write(CLOG_ERR, fmt, ...)
CLOG_WARN(fmt, ...)  → clog_write(CLOG_WARN, fmt, ...)
CLOG_INFO(fmt, ...)  → clog_write(CLOG_INFO, fmt, ...)
CLOG_DEBUG(fmt, ...) → clog_write(CLOG_DEBUG, fmt, ...)
```

All macros follow the same pattern: expand to a `clog_write()`
call with the appropriate level prepended.

### Compile-Time Control Flags

```
-DCLOG_STRIP
  Effect: All CLOG_* macros expand to ((void)0)
  Use:    Release builds with zero logging overhead

-DCLOG_MIN_LEVEL=N
  Effect: Macros for levels > N expand to ((void)0)
  Use:    Keep ERR+WRN (N=2) while stripping INF+DBG
  Values: 1=ERR only, 2=ERR+WRN, 3=ERR+WRN+INF, 4=all
```

`CLOG_STRIP` takes precedence over `CLOG_MIN_LEVEL` (if both
defined, all logging is stripped).

## Platform Behavioral Differences

| Behavior | POSIX | Classic Mac |
|----------|-------|-------------|
| Default output | stderr | Text file named after app |
| Line ending | `\n` | `\r\n` |
| Buffer size | 256 bytes | 192 bytes |
| Timestamp source | gettimeofday() delta | TickCount() delta (ms) |
| Flush strategy | Line buffered | Immediate (FSWrite per line) |
| File creation | fopen() | Create + FSOpen |
| File type/creator | N/A | 'TEXT' / 'CLog' |
