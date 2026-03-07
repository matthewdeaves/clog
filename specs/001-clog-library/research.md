# Research: clog Logging Library

**Phase**: 0 — Outline & Research
**Date**: 2026-02-28

## Research Summary

This feature has no unresolved NEEDS CLARIFICATION items. The
existing detailed specification (`spec.md` at repo root) and
constitution provide complete technical direction. Research
focused on confirming implementation patterns.

## Decisions

### D1: Variadic Macro Strategy for C89

**Decision**: Use a wrapper function approach for C89 compilers
that lack variadic macro support (`__VA_ARGS__`).

**Rationale**: C89 does not define variadic macros. However,
most C89-era compilers (including Retro68's GCC) support them
as an extension. The header provides both paths: variadic macros
when supported, and a fallback using `do { } while(0)` wrapping
a direct `clog_write()` call when not.

**Alternatives considered**:
- Double-parenthesis trick `CLOG_INFO(("fmt", args))` — rejected
  because it requires an extra layer of parentheses in user code,
  violating the "simple API" goal.
- Function-only API (no macros) — rejected because compile-time
  stripping requires macros; functions cannot be stripped to
  `((void)0)`.

### D2: Timestamp Source

**Decision**: Use `gettimeofday()` on POSIX and `TickCount()` on
Classic Mac, both computing delta from init time.

**Rationale**: Both are available without additional dependencies.
`gettimeofday()` gives microsecond precision (we display
milliseconds). `TickCount()` gives ~16.7ms resolution (1/60th
second ticks), which is adequate for Classic Mac logging.

**Alternatives considered**:
- `clock_gettime()` on POSIX — not C89 portable, requires
  `_POSIX_C_SOURCE` or linking `-lrt`.
- `Microseconds()` on Mac — only available on later System
  versions, not universally present on System 6 targets.

### D3: Static Buffer Sizing

**Decision**: 256 bytes on POSIX, 192 bytes on Mac.

**Rationale**: 256 bytes is generous for single-line log messages
on modern systems. 192 bytes on Mac fits in a 68030 data cache
line and keeps stack usage low on memory-constrained systems.
Both include space for the `[timestamp][LVL] ` prefix.

**Alternatives considered**:
- Configurable buffer via `#define` — adds complexity for
  negligible benefit; users who need larger messages should
  rethink their logging, not increase the buffer.
- Shared single size (e.g., 256 for both) — wastes precious
  stack on 68k Macs with 32KB or 128KB stack limits.

### D4: File Management on Classic Mac

**Decision**: Use `Create`/`FSOpen`/`FSWrite`/`SetFPos`/`FSClose`
from the File Manager. Creator type `'CLog'`, file type `'TEXT'`.

**Rationale**: These are the lowest-level, most portable File
Manager calls available from System 6 onward. Higher-level
APIs like `FSpOpenDF` require System 7+.

**Alternatives considered**:
- `FSpCreate`/`FSpOpenDF` — System 7+ only, breaks System 6
  compatibility.
- Standard C `fopen`/`fwrite` — not available in Classic Mac
  Retro68 runtime for all targets.

### D5: Build System

**Decision**: CMake with platform detection. POSIX builds use
host compiler. 68k and PPC builds use Retro68 toolchain files.

**Rationale**: CMake supports Retro68 toolchain files. Retro68
provides ready-made toolchain files. A single `CMakeLists.txt`
handles all three targets by selecting the correct `.c` file
based on toolchain detection.

**Implementation notes**:
- Each Retro68 toolchain sets a different `CMAKE_SYSTEM_NAME`:
  - 68k: `Retro68`
  - PPC: `RetroPPC`
- Platform detection must match both: `CMAKE_SYSTEM_NAME MATCHES "Retro68|RetroPPC"`
- Toolchain files at:
  - `~/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake`
  - `~/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake`

**Alternatives considered**:
- Plain Makefile — would need three separate Makefiles or
  complex conditional logic; CMake handles this cleanly.
- Meson — not supported by Retro68 toolchain files.

---

## Feedback Findings (Consumer Integration Testing)

### R1: POSIX Timestamp Overflow on Second Boundary

**Decision**: The microsecond delta computation in clog_posix.c must
handle borrow when now.tv_usec < start_time.tv_usec. The current
unsigned cast wraps negative values to near-UINT64_MAX.

**Rationale**: After the first second boundary, every timestamp is
garbage. This makes POSIX log output unreadable in practice.

**Source**: Consumer integration testing — observed timestamps
jumping from 2ms to 18446744073717502 after ~1 second.

### R2: Variadic Macro Pragma Suppression

**Decision**: clog.h should wrap its macro definitions in
`#pragma GCC diagnostic ignored "-Wvariadic-macros"` so consumers
using `-pedantic` don't need workarounds.

**Rationale**: Every C89 consumer using -pedantic must either remove
the flag or add pragma wrappers around `#include "clog.h"`. This
burden should be on the library, not consumers.

**Source**: Consumer had to suppress warnings in its own headers
when including clog.h with -pedantic.

### R3: clog_set_file() Silent Failure

**Decision**: clog_set_file() should return int (0 success, -1 if
already initialized) instead of void, giving consumers error feedback.

**Rationale**: On Classic Mac where file output is the only channel,
calling set_file after init means zero output with no indication of
what went wrong. Silent failure violates least-surprise.

**Source**: Repeated debugging sessions during consumer hardware
testing traced to wrong call ordering with no error signal.

### R4: CMake add_subdirectory() Support

**Decision**: Guard test targets with option(CLOG_BUILD_TESTS) so
consumers can include clog via add_subdirectory() without building tests.

**Rationale**: Without this, consumers must pre-build clog separately
per platform. With it, consumer CMake does: set(CLOG_BUILD_TESTS OFF),
add_subdirectory(clog), target_link_libraries(myapp PRIVATE clog).

**Source**: Consumer build system required multiple separate
find_library blocks for cross-platform clog linking.

### R5: Format Attribute for clog_write

**Decision**: Add `__attribute__((format(printf, 2, 3)))` (guarded by
`__GNUC__`) to clog_write declaration.

**Rationale**: Without it, `CLOG_INFO("literal")` with no format args
triggers -Wformat-security. Consumers resort to `CLOG_INFO("%s", "text")`
everywhere. The attribute tells GCC the format is validated, suppressing
false positives while catching real format bugs.

**Source**: Dozens of awkward `"%s", "text"` patterns in consumer code.

### R6: Classic Mac MaxApplZone() in clog_init()

**Decision**: Call MaxApplZone() inside clog_init() on Classic Mac.
MaxApplZone() is idempotent — if the zone already extends to the
limit, it won't be changed (Inside Macintosh Volume I).

**Rationale**: File Manager calls (Create/FSOpen) use system heap,
not app heap, so MaxApplZone is not strictly required for them.
However, on cold boot with code segments loading, heap fragmentation
can cause crashes during or near clog_init(). Calling MaxApplZone()
eliminates this crash vector with zero downside since it is idempotent.

**Source**: Consumer hardware testing on 68k Macs — crashes near init
traced to unexpanded heap zone. Inside Macintosh confirms MaxApplZone
is safe to call multiple times.

### R7: Append Mode for Log Files

**Decision**: Add clog_set_append(int enable) function, called before
init like clog_set_file. When enabled, skip truncation (SetEOF on Mac,
"a" mode on POSIX).

**Rationale**: When running multiple test apps sequentially on Classic
Mac, each overwrites the previous log file. Per-app filenames work but
append mode is simpler for sequential testing.

**Source**: Consumer worked around this with per-app log filenames.

### R8: Memory Footprint Documentation

**Decision**: Document clog's static memory usage in clog.h header
and README.md.

**Rationale**: Classic Mac consumers need to budget heap via SIZE
resource. clog uses ~250 bytes static on Mac (50B state + 192B buffer),
~310 bytes on POSIX (50B state + 256B buffer). No heap allocation.

**Source**: Consumer allocated large heap partly to cover clog without
knowing actual requirements.
