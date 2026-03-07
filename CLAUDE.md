# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

clog — Minimal C89 logging library for Classic Macintosh and modern systems. One header (`clog.h`), two implementations (`clog_posix.c`, `clog_mac.c`). Standalone library with zero consumer dependencies.

## Build & Test

```bash
# POSIX build + test
mkdir -p build && cd build && cmake .. && make && ctest --output-on-failure

# 68k cross-compile (produces libclog.a only, no tests)
mkdir -p build-68k && cd build-68k
cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake && make

# PPC cross-compile (produces libclog.a only, no tests)
mkdir -p build-ppc && cd build-ppc
cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake && make
```

## Code Constraints

- **C89/C90 strict** — no `//` comments, no mixed declarations, no VLAs
- **`-pedantic` safe** — clog.h wraps variadic macros in `#pragma GCC diagnostic` suppression; consumers can use `-pedantic`
- **No dynamic allocation** — static buffers only (256B POSIX, 192B Mac)
- **POSIX impl** needs `#define _POSIX_C_SOURCE 200112L` before includes for vsnprintf
- **Under 500 lines total** across clog.h + clog_posix.c + clog_mac.c
- **Not interrupt-safe** — never call from ASR/notifier/ISR

## Architecture

- `include/clog.h` — public API: 6 functions, 4 convenience macros, compile-time stripping via `CLOG_STRIP` / `CLOG_MIN_LEVEL`
- `src/clog_posix.c` — fprintf to stderr/file, gettimeofday() timestamps
- `src/clog_mac.c` — File Manager (Create/FSOpen/FSWrite/FSClose), TickCount() timestamps
- `tests/test_clog.c` — POSIX-only test suite, returns 0/1
- `CMakeLists.txt` — selects implementation via `CMAKE_SYSTEM_NAME MATCHES "Retro68|RetroPPC"`, supports `add_subdirectory()` via `CLOG_BUILD_TESTS` option

## Retro68 Toolchain

- 68k: `CMAKE_SYSTEM_NAME` = `Retro68`, toolchain = `retro68.toolchain.cmake`
- PPC: `CMAKE_SYSTEM_NAME` = `RetroPPC`, toolchain = `retroppc.toolchain.cmake`

## Spec Artifacts

Design docs in `specs/001-clog-library/`: spec.md, plan.md, tasks.md, research.md, contracts/, data-model.md, quickstart.md. Constitution at `.specify/memory/constitution.md` (v1.1.0, 9 principles).
