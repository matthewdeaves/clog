<!--
Sync Impact Report
===================
Version change: 1.0.0 → 1.1.0
Source: constitution review during speckit tooling setup

Modified principles:
  - II: Removed PeerTalk-specific rationale, made standalone
  - VIII (new): "Standalone Library, Zero Consumer Dependencies"
  - IX (was VIII): "Keep It Tiny" renumbered

Modified sections:
  - Definition of Done: Replaced "PeerTalk can link" with
    "Any C89 project can link", added clog_set_file per-app
    log filename criterion

Templates requiring updates: None (Constitution Check gate is generic)

Follow-up TODOs: None
-->

# clog Constitution

## Core Principles

### I. One Header, Two Implementations

`clog.h` is the sole public API. `clog_posix.c` uses stdio.
`clog_mac.c` uses File Manager. Application code MUST NOT
contain platform ifdefs for logging. The build system selects
exactly one implementation per target.

### II. Four Levels, No Categories

Error, warn, info, debug. That is the complete level set.
Category or tag filtering MUST NOT be added. If finer
granularity is needed, encode it in the message text.
Rationale: categories add complexity without value — no
consumer of clog has ever filtered by category.

### III. ISR-Safe by Prevention

clog is NOT safe to call from interrupt context (MacTCP ASR,
OT notifier, or any ISR). This rule is enforced by
documentation and code review, not by runtime checks. The
correct pattern: set a flag in the interrupt handler, log
from the main loop.

### IV. Compile-Time Stripping

`-DCLOG_STRIP` MUST remove all logging at compile time with
zero overhead. `-DCLOG_MIN_LEVEL=N` MUST remove macros below
level N. When stripped, all `CLOG_*` macros expand to
`((void)0)`.

### V. No Dynamic Allocation

A single static buffer is used for formatting. No `malloc`,
no `NewPtr`, no handles. The buffer is sized at compile time:
256 bytes on POSIX, 192 bytes on Mac. Messages exceeding the
buffer are silently truncated.

### VI. C89 for Portability

The header and both implementations MUST compile as strict
C89/C90. This ensures compatibility from Retro68 (Classic Mac
68k/PPC cross-compiler) through modern GCC/Clang.

### VII. File Output on Mac, stderr on POSIX

Mac apps write to a text file via File Manager. POSIX apps
write to stderr. Both destinations can be overridden at init
time via `clog_set_file`. No callbacks, no multiple
simultaneous outputs.

### VIII. Standalone Library, Zero Consumer Dependencies

clog is a standalone library. It MUST NOT depend on, import
from, or reference any consumer (including PeerTalk). Any
project MAY link against libclog.a. clog's API, build system,
and spec artifacts are self-contained.

### IX. Keep It Tiny

Target: under 500 lines total across header + both
implementations. If the codebase grows past that threshold,
something is wrong. Each file MUST fit on roughly one screen
of code.

## Deliverables & Exclusions

### What Ships

- `clog.h` -- single public header
- `clog_posix.c` -- POSIX implementation
- `clog_mac.c` -- Classic Mac implementation
- Static library (`libclog.a`) per platform
- CMake build with Retro68 toolchain support

### What Does Not Ship

These features are explicitly out of scope and MUST NOT be
added:

- Thread safety (single-threaded design)
- Category/tag filtering
- Callback or hook system
- Structured/binary/performance logging
- Ring buffers or log rotation
- Runtime level changes after init
- Multiple simultaneous outputs

## Definition of Done

clog is done when:

- Any C89 project can link against libclog.a on POSIX, 68k,
  and PPC without modification
- `CLOG_INFO("hello %s", name)` writes to stderr on Linux
  and a file on Mac
- `-DCLOG_STRIP` produces zero logging code
- The whole thing fits in one screen of code per file
- All three build targets (POSIX, 68k, PPC) produce a
  working `libclog.a`
- `clog_set_file()` enables per-app log filenames so
  multiple apps on the same Mac can log without collision

## Governance

This constitution is the authoritative reference for clog
design decisions. All implementation work MUST comply with
the Core Principles above.

- **Amendments**: Changes to principles require updating this
  document with a version bump and amended date. Principle
  removals or redefinitions require a MAJOR version bump.
  New principles or material expansions require MINOR. Wording
  or typo fixes require PATCH.
- **Compliance**: Feature specs and implementation plans MUST
  be checked against Core Principles before work begins
  (Constitution Check gate in plan template).
- **Exclusions are final**: Items listed under "What Does Not
  Ship" MUST NOT be implemented unless the constitution is
  formally amended first.
- **Guidance**: CLAUDE.md serves as the runtime development
  guidance file for build commands and project structure.

**Version**: 1.1.0 | **Ratified**: 2026-02-28 | **Last Amended**: 2026-03-07
