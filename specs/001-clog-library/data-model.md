# Data Model: clog Logging Library

**Phase**: 1 — Design & Contracts
**Date**: 2026-02-28

## Entities

### ClogLevel (Enumeration)

Represents the four log severity tiers.

| Value | Name  | Abbreviation | Numeric |
|-------|-------|--------------|---------|
| 1     | Error | ERR          | 1       |
| 2     | Warn  | WRN          | 2       |
| 3     | Info  | INF          | 3       |
| 4     | Debug | DBG          | 4       |

**Ordering rule**: Lower numeric value = higher severity.
A message is output only when its level value <= the
configured minimum level value.

### Logger State (Internal, Not Exposed)

Single static instance per process. Not user-accessible.

| Field | Description | Constraints |
|-------|-------------|-------------|
| app_name | Application identifier | Truncated to 31 chars + null terminator (32 bytes) |
| min_level | Minimum severity for output | ClogLevel value (1-4) |
| initialized | Whether init has been called | Boolean (0 or 1) |
| start_time | Timestamp at init | Platform-specific (timeval on POSIX, unsigned long ticks on Mac) |
| output_file | Override filename | NULL for default; set before init |
| file_handle | Open file descriptor/refNum | Platform-specific; -1/0 when unset |

**Lifecycle**:
1. All fields zero-initialized at program start
2. `clog_set_file()` sets `output_file` (before init only)
3. `clog_init()` populates all fields, opens output
4. `clog_write()` reads fields, writes formatted message
5. `clog_shutdown()` closes file handle, clears initialized

### Log Message (Transient)

Formatted into a static buffer on each `clog_write()` call.
Never stored beyond the write operation.

**Format**: `[<timestamp_ms>][<LVL>] <user_message><newline>`

| Component | Size | Description |
|-----------|------|-------------|
| `[` | 1 byte | Opening bracket |
| timestamp | 1-10 digits | Milliseconds since init |
| `][` | 2 bytes | Separator |
| level | 3 chars | ERR, WRN, INF, DBG |
| `] ` | 2 bytes | Closing bracket + space |
| message | remaining | User format string expanded |
| newline | 1-2 bytes | `\n` (POSIX) or `\r\n` (Mac) |

**Buffer sizes**: 256 bytes (POSIX), 192 bytes (Mac).
Overflow is silently truncated.

## State Transitions

```text
         clog_set_file()
              │ (optional, before init only)
              ▼
 UNINITIALIZED ──── clog_init() ───► ACTIVE
       ▲                               │
       │                               │ clog_write() loops
       │                               │ (filters by level)
       │                               ▼
       └────── clog_shutdown() ◄── ACTIVE
```

- `clog_write()` in UNINITIALIZED state: no-op
- `clog_shutdown()` in UNINITIALIZED state: no-op (safe)
- `clog_shutdown()` called twice: second call is no-op
- `clog_set_file()` in ACTIVE state: ignored
