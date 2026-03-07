# Implementation Plan: clog Logging Library

**Branch**: `001-clog-library` | **Date**: 2026-02-28 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-clog-library/spec.md`

## Summary

Implement a minimal C89 logging library with one public header
(`clog.h`) and two platform implementations (`clog_posix.c` for
stderr/file output via stdio, `clog_mac.c` for File Manager output
on Classic Mac). The library provides 6 functions and 4 convenience
macros with compile-time stripping support. Builds as `libclog.a`
on POSIX, 68k Mac, and PPC Mac via CMake with Retro68 toolchains.

## Technical Context

**Language/Version**: C89/C90 (strict)
**Primary Dependencies**: None (standard C library only; Mac Toolbox
for Classic Mac implementation)
**Storage**: stderr (POSIX default), text file via File Manager
(Mac default), optional file override on both platforms
**Testing**: Custom C test program (`test_clog.c`), run via
CMake/CTest on POSIX
**Target Platform**: Linux/POSIX, Classic Mac 68k (Retro68),
Classic Mac PPC (Retro68)
**Project Type**: Static library
**Performance Goals**: <1ms per formatted log write
**Constraints**: No dynamic allocation, <500 lines total across
all source files, C89 compatible, single-threaded only
**Scale/Scope**: 6 functions, 4 macros, 3 source files (1 header +
2 implementations), 1 test file, 1 CMake build file

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Status | Notes |
|---|-----------|--------|-------|
| I | One Header, Two Implementations | PASS | `clog.h` + `clog_posix.c` + `clog_mac.c` |
| II | Four Levels, No Categories | PASS | ERR, WRN, INF, DBG only |
| III | ISR-Safe by Prevention | PASS | Documented in header; no runtime ISR checks |
| IV | Compile-Time Stripping | PASS | `CLOG_STRIP` and `CLOG_MIN_LEVEL` macros |
| V | No Dynamic Allocation | PASS | Static buffer only (256B POSIX, 192B Mac) |
| VI | C89 for Portability | PASS | All code strict C89/C90 |
| VII | File on Mac, stderr on POSIX | PASS | Platform-specific defaults with `clog_set_file` override |
| VIII | Standalone Library | PASS | No consumer dependencies |
| IX | Keep It Tiny | PASS | Target <500 lines total (est. ~450 after Phase 8) |

**Gate result**: ALL PASS. No violations to justify.

## Project Structure

### Documentation (this feature)

```text
specs/001-clog-library/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   └── clog-api.md      # Public C API contract
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code (repository root)

```text
include/
└── clog.h               # Public API header

src/
├── clog_posix.c          # POSIX implementation (stdio)
└── clog_mac.c            # Classic Mac implementation (File Manager)

tests/
└── test_clog.c           # Basic test program

CMakeLists.txt            # Build config (POSIX + Retro68 toolchains)
```

**Structure Decision**: Flat single-library layout matching CLAUDE.md.
Header in `include/` for standard `-I` include path usage. Both
implementations in `src/`, build system selects one per target.
Tests in `tests/`, one test file sufficient for this scope.

## Feedback Constraints (2026-03-07)

- POSIX timestamp arithmetic must handle microsecond borrow
- clog.h must internally suppress -Wvariadic-macros for -pedantic consumers
- clog_set_file() signature changes from void to int (API-compatible: callers ignoring return value still compile)
- CMake must support both standalone build and add_subdirectory() usage
- clog_write() needs __attribute__((format(printf,2,3))) guarded by __GNUC__
- Classic Mac init must call MaxApplZone() before File Manager operations
- New function clog_set_append() added to public API (6 functions total)
- Memory footprint documented: ~250B Mac, ~310B POSIX (static, no heap)

## Complexity Tracking

> No violations. Table intentionally empty.
