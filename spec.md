# clog Specification

## 1. Public API

### Types

```c
typedef enum {
    CLOG_LVL_ERR  = 1,   /* Errors only */
    CLOG_LVL_WARN = 2,   /* Warnings + errors */
    CLOG_LVL_INFO = 3,   /* Info + warnings + errors (default) */
    CLOG_LVL_DBG  = 4    /* Everything */
} ClogLevel;

/* Flush modes */
#define CLOG_FLUSH_NONE   0  /* No flush after write (fastest, default) */
#define CLOG_FLUSH_ALL    1  /* Flush to disk after every write */
#define CLOG_FLUSH_ERRORS 2  /* Flush to disk for ERR and WARN only */
```

### Functions

```c
/* Lifecycle */
int         clog_init(const char *app_name, ClogLevel level);
void        clog_shutdown(void);

/* Output control (call before clog_init, or ignored) */
int         clog_set_file(const char *filename);
int         clog_set_append(int enable);
int         clog_set_flush(int mode);

/* Runtime control */
void        clog_set_level(ClogLevel level);

/* Network sink — callback receives each formatted log line */
void        clog_set_network_sink(ClogNetworkSink fn, void *user_data);

/* Logging */
void        clog_write(ClogLevel level, const char *fmt, ...);

/* Utility */
const char *clog_level_name(ClogLevel level);
```

9 functions total.

### Macros

```c
CLOG_ERR(fmt, ...)    /* clog_write(CLOG_LVL_ERR, fmt, ...) */
CLOG_WARN(fmt, ...)   /* clog_write(CLOG_LVL_WARN, fmt, ...) */
CLOG_INFO(fmt, ...)   /* clog_write(CLOG_LVL_INFO, fmt, ...) */
CLOG_DEBUG(fmt, ...)  /* clog_write(CLOG_LVL_DBG, fmt, ...) */
```

### Compile-Time Control

```c
-DCLOG_STRIP           /* Remove all logging (macros expand to nothing) */
-DCLOG_MIN_LEVEL=2     /* Only include ERR and WARN */
```

When `CLOG_STRIP` is defined, all `CLOG_*` macros expand to `((void)0)`. When `CLOG_MIN_LEVEL` is defined, macros below that level expand to `((void)0)`.

### C89 Compatibility

The public API uses only C89 types. The variadic macros use C89's `do { } while(0)` pattern wrapping a call to `clog_write`. On compilers without variadic macro support, the macros call through a function taking the level as first argument.

## 2. Behavior

### Init

`clog_init(app_name, level)`:
- Stores the app name (truncated to 31 chars) and minimum log level
- On POSIX: opens stderr (default) or file if `clog_set_file` was called first
- On Mac: opens/creates a text file named `app_name` (no extension) using File Manager (`FSOpen`/`Create`). Creator type `'CLog'`, file type `'TEXT'`
- Clears the file on each init (fresh log per run), unless `clog_set_append(1)` was called
- Returns 0 on success, -1 on failure

### Writing

`clog_write(level, fmt, ...)`:
- If `level > current_level`, return immediately (filtered out)
- Format message into static buffer using `vsprintf` (384 bytes on Mac, 256 on POSIX)
- Prepend timestamp and level tag: `[timestamp][LEVEL] message\n`
- Write to output (stderr or file)
- If a network sink is registered, also deliver the formatted line to the callback
- On Mac: use `FSWrite`, append `\r\n` (Mac line ending)
- On POSIX: use `fprintf(stderr, ...)`, append `\n`

### Timestamp

- POSIX: milliseconds since init, via `gettimeofday()`
- Mac: ticks since init via `TickCount()`, converted to milliseconds

### Output Format

```
[1234][INF] Connected to PlayerTwo
[1250][ERR] Send failed: -23008
[1251][DBG] Buffer at 1024/4096 bytes
```

Timestamp is milliseconds since `clog_init`. Level is 3-char abbreviation: `ERR`, `WRN`, `INF`, `DBG`.

### File Output

`clog_set_file(filename)`:
- On POSIX: subsequent writes go to this file instead of stderr
- On Mac: overrides the default filename (normally `app_name`)
- Must be called before `clog_init`, or has no effect
- Pass NULL to revert to default (stderr on POSIX, app_name on Mac)

### Append Mode

`clog_set_append(enable)`:
- When enabled (1), `clog_init` preserves existing file contents instead of truncating
- Must be called before `clog_init`, or has no effect
- Default: disabled (fresh log each run)

### Flush Mode

`clog_set_flush(mode)`:
- `CLOG_FLUSH_NONE` (0): no flush after write (fastest, default)
- `CLOG_FLUSH_ALL` (1): flush to disk after every write
- `CLOG_FLUSH_ERRORS` (2): flush to disk for ERR and WARN only
- Must be called before `clog_init`, or has no effect

### Runtime Level Control

`clog_set_level(level)`:
- Changes the minimum log level at runtime (may be called after init)
- Messages with level > new level are filtered out

### Network Sink

`clog_set_network_sink(fn, user_data)`:
- Registers a callback that receives each formatted log line
- Called after formatting but before file write
- Pass NULL to disable
- Used by PeerTalk to bridge clog output to its debug broadcast channel

### Shutdown

`clog_shutdown()`:
- On POSIX: close file if one was opened (not stderr)
- On Mac: close file via `FSClose`
- Safe to call multiple times

### ISR Safety

clog is NOT interrupt-safe. Never call from MacTCP ASR, OT notifier, or any interrupt context. The correct pattern:

```c
/* In ASR/notifier — just set a flag */
state->error_occurred = 1;

/* In main loop — log it */
if (state->error_occurred) {
    state->error_occurred = 0;
    CLOG_ERR("Network error in callback");
}
```

### Message Size Limits

- POSIX: 256 bytes max per message (including timestamp/level prefix)
- Mac: 384 bytes max (including timestamp/level prefix)
- Messages exceeding the buffer are silently truncated

## 3. Platform Implementations

### POSIX (`clog_posix.c`)

- Output: `fprintf(stderr, ...)` or `fprintf(file, ...)`
- Timestamp: `gettimeofday()` delta from init time
- No thread safety (single-threaded design)
- Line buffered output

### Classic Mac (`clog_mac.c`)

- Output: File Manager (`FSOpen`, `FSWrite`, `SetFPos`, `FSClose`)
- File created with `Create` then opened with `FSOpen`
- Timestamp: `TickCount()` delta from init, converted to ms
- Creator: `'CLog'`, Type: `'TEXT'`
- Each write calls `FSWrite` immediately (no buffering — Mac apps may crash, so flush every line)
- `clog_init` truncates file to zero (`SetEOF(refNum, 0)`) for fresh log each run (unless append mode)

## 4. Build

- Static library: `libclog.a`
- One .c file compiled per platform (selected by build system)
- CMake with Retro68 toolchain support
- Header-only mode possible: define `CLOG_IMPLEMENTATION` in one .c file to include the implementation (optional convenience, not required)

## 5. What's Out of Scope

- Thread safety
- Category/tag filtering
- Multiple simultaneous outputs
- Structured/binary logging
- Log rotation or size limits
