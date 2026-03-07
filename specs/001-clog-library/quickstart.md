# Quickstart: clog Logging Library

**Phase**: 1 — Design & Contracts
**Date**: 2026-02-28

## 1. Add clog to your project

Include the clog header in any source file that needs logging:

```c
#include "clog.h"
```

Link against the static library (`libclog.a`) for your target
platform.

## 2. Initialize

At application startup, call `clog_init` with your app name
and desired minimum log level:

```c
int main(void) {
    if (clog_init("MyApp", CLOG_INFO) != 0) {
        /* init failed — handle error */
        return 1;
    }

    /* ... application code ... */

    clog_shutdown();
    return 0;
}
```

## 3. Log messages

Use the four convenience macros anywhere after init:

```c
CLOG_ERR("Connection failed: %d", error_code);
CLOG_WARN("Retrying in %d seconds", delay);
CLOG_INFO("Connected to %s", peer_name);
CLOG_DEBUG("Buffer: %d/%d bytes", used, total);
```

Output looks like:

```
[1234][ERR] Connection failed: -23008
[1240][WRN] Retrying in 5 seconds
[1250][INF] Connected to PlayerTwo
[1251][DBG] Buffer: 1024/4096 bytes
```

The number in the first bracket is milliseconds since
`clog_init()` was called.

## 4. Filter by level

The level you pass to `clog_init` controls what gets output:

| Level | What you see |
|-------|-------------|
| `CLOG_ERR` | Errors only |
| `CLOG_WARN` | Errors + warnings |
| `CLOG_INFO` | Errors + warnings + info (recommended) |
| `CLOG_DEBUG` | Everything |

## 5. Redirect output to a file (optional)

By default, POSIX builds write to stderr and Mac builds write
to a file named after the app. To override:

```c
clog_set_file("debug.log");  /* Must be called BEFORE init */
clog_init("MyApp", CLOG_DEBUG);
```

Pass `NULL` to revert to the platform default:

```c
clog_set_file(NULL);
```

To append to an existing log file instead of overwriting:

```c
clog_set_file("debug.log");
clog_set_append(1);
clog_init("MyApp", CLOG_DEBUG);
```

## 6. Strip logging for release builds

Add a compiler flag to remove all logging at compile time:

```
-DCLOG_STRIP
```

Or keep only errors and warnings:

```
-DCLOG_MIN_LEVEL=2
```

When stripped, all `CLOG_*` macros expand to nothing — zero
runtime overhead.

## 7. Build

### POSIX (Linux, macOS, etc.)

```bash
mkdir build && cd build
cmake ..
make
```

### Classic Mac 68k

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make
```

### Classic Mac PPC

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
make
```

All targets produce `libclog.a`.

## 8. Important notes

- **Not interrupt-safe**: Never call `CLOG_*` from an ISR,
  ASR, or OT notifier. Set a flag, log from the main loop.
- **Single-threaded**: No thread safety.
- **Buffer limits**: Messages are truncated at 256 bytes
  (POSIX) or 192 bytes (Mac).
- **One init per run**: Call `clog_init` once at startup and
  `clog_shutdown` once at exit.
- **Memory footprint**: ~250 bytes static on Classic Mac (50B state
  + 192B buffer), ~310 bytes on POSIX (50B state + 256B buffer).
  Zero heap allocation.
