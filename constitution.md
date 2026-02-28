# clog Constitution

## What clog Is

A minimal C logging library for Classic Macintosh and modern systems. One header, two implementations.

## What It's For

PeerTalk (and any other Classic Mac C project) needs logging that works on System 6 through modern Linux. clog is that logging library.

## What the Developer Sees

```c
#include "clog.h"

clog_init("MyApp", CLOG_INFO);

CLOG_INFO("Connected to %s", peer_name);
CLOG_ERR("Send failed: %d", err);
CLOG_DEBUG("Buffer at %d/%d bytes", used, total);

clog_shutdown();
```

That's the whole API for 90% of usage. Init, log, shutdown.

## Principles

1. **One header, two implementations.** `clog.h` is the public API. `clog_posix.c` uses stdio. `clog_mac.c` uses File Manager. No ifdefs in application code.

2. **Four levels, no categories.** Error, warn, info, debug. If you need categories, use the message text. Categories were over-engineered in v1 — none of the three PeerTalk apps ever filtered by category.

3. **ISR-safe by prevention.** clog is NOT safe to call from interrupt context. The ISR safety rule is enforced by documentation and code review, not by runtime checks. If you're in an ASR/notifier, don't call clog. Set a flag, log from the main loop.

4. **Compile-time stripping.** `-DCLOG_STRIP` removes all logging at compile time. Zero overhead in release builds. `-DCLOG_MIN_LEVEL=2` keeps only error and warn.

5. **No dynamic allocation.** A single static buffer for formatting. No malloc, no NewPtr, no handles. The buffer is sized at compile time (256 bytes on POSIX, 192 bytes on Mac).

6. **C89 for portability.** The header and both implementations compile as C89/C90.

7. **File output on Mac, stderr on POSIX.** Mac apps write to a text file (File Manager). POSIX apps write to stderr. Both can be changed at init time. No callbacks, no multiple outputs.

8. **Keep it tiny.** Target: under 500 lines total across header + both implementations. If it's growing past that, something is wrong.

## What Ships

- `clog.h` — single public header
- `clog_posix.c` — POSIX implementation
- `clog_mac.c` — Classic Mac implementation
- Static library (`libclog.a`) per platform

## What Doesn't Ship

- Thread safety (single-threaded design, same as PeerTalk)
- Category filtering
- Callback/hook system
- Performance/structured logging
- Ring buffers, log rotation
- Runtime level changing (set once at init)

## Definition of Done

clog is done when:
- PeerTalk can link against it on POSIX, 68k, and PPC
- `CLOG_INFO("hello %s", name)` writes to stderr on Linux and a file on Mac
- `-DCLOG_STRIP` produces zero logging code
- The whole thing fits in one screen of code per file
