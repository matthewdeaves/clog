# Tasks: clog Logging Library

**Input**: Design documents from `/specs/001-clog-library/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/

**Tests**: A basic test program (test_clog.c) is included as part of the US1 verification, per the implementation plan. No TDD approach was requested.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Single project**: `include/`, `src/`, `tests/` at repository root
- Header in `include/` for standard `-I` include path
- One implementation file per platform in `src/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project directory structure and build system

- [x] T001 Create directory structure: `include/`, `src/`, `tests/` at repository root
- [x] T002 Create CMakeLists.txt at repository root with: `project(clog)`, C language, C89/C90 standard flags (`-std=c89 -pedantic -Wall -Werror`), static library target `clog` from `src/clog_posix.c` (default) or `src/clog_mac.c` (when Retro68 toolchain detected), include directory `include/`, test executable from `tests/test_clog.c` linked to `clog`, Retro68 toolchain detection via `CMAKE_SYSTEM_NAME MATCHES "RetroMacOS"` to select Mac source file

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Public header that ALL user stories depend on

- [x] T003 Write include/clog.h with: include guard (`CLOG_H`), `ClogLevel` enum (CLOG_ERR=1, CLOG_WARN=2, CLOG_INFO=3, CLOG_DEBUG=4), declarations for all 5 functions (`clog_init`, `clog_shutdown`, `clog_set_file`, `clog_write`, `clog_level_name`), 4 convenience macros (`CLOG_ERR`, `CLOG_WARN`, `CLOG_INFO`, `CLOG_DEBUG`) expanding to `clog_write()` calls, `CLOG_STRIP` logic (all macros expand to `((void)0)`), `CLOG_MIN_LEVEL` logic (macros for levels > N expand to `((void)0)`), `CLOG_STRIP` takes precedence over `CLOG_MIN_LEVEL`, ISR safety warning comment in header doc block

**Checkpoint**: Header complete — all user story implementation can begin

---

## Phase 3: User Story 1 - Basic Log Lifecycle (Priority: P1) MVP

**Goal**: A developer can init, log at four levels with timestamps, and shutdown on POSIX

**Independent Test**: Call init with INFO level, log one message at each level, verify `[ms][LVL] message` format appears on stderr, call shutdown

### Implementation for User Story 1

- [x] T004 [US1] Implement src/clog_posix.c with: static `clog_state` struct (app_name[32], min_level, initialized flag, start_time via `struct timeval`, output FILE pointer, custom filename pointer), `clog_init()` storing app_name (strncpy, 31 char limit), level, `gettimeofday()` start time, opening stderr or custom file, returning 0/-1, `clog_write()` checking initialized and level filter, computing ms delta via `gettimeofday()`, formatting with `vsnprintf` into 256-byte static buffer as `[%lu][%s] ` prefix + message + `\n`, writing via `fprintf`, `clog_shutdown()` closing file (if not stderr), clearing initialized flag (idempotent), `clog_set_file()` storing filename pointer (ignored if already initialized), `clog_level_name()` returning "ERR"/"WRN"/"INF"/"DBG"/"???"
- [x] T005 [US1] Create tests/test_clog.c with: test init returns 0 on success, test logging at each level with output redirected to a temp file via `clog_set_file`, verify output lines match `[<digits>][LVL] <message>` format, test level filtering (init at WARN, verify DEBUG/INFO produce no output), test double-shutdown does not crash, test logging before init is a no-op, return 0 on success or 1 on any failure
- [x] T006 [US1] Build and run POSIX test: `mkdir -p build && cd build && cmake .. && make && ctest --output-on-failure`

**Checkpoint**: POSIX logging works end-to-end. MVP complete.

---

## Phase 4: User Story 2 - Classic Mac File Output (Priority: P2)

**Goal**: A developer can init, log, and shutdown on Classic Mac with output to a text file via File Manager

**Independent Test**: Build with Retro68 toolchain, init clog on Mac target, verify text file created with correct format and `\r\n` line endings

### Implementation for User Story 2

- [x] T007 [US2] Implement src/clog_mac.c with: static `clog_state` struct (app_name[32], min_level, initialized flag, start_ticks via `unsigned long` from `TickCount()`, `short refNum` file reference, custom filename pointer), `clog_init()` storing app_name, level, calling `Create()` then `FSOpen()` for log file (creator `'CLog'`, type `'TEXT'`), `SetEOF(refNum, 0)` to truncate, `clog_write()` checking initialized and level filter, computing ms delta as `(TickCount() - start_ticks) * 50 / 3`, formatting with `vsprintf` into 192-byte static buffer as `[%lu][%s] ` prefix + message + `\r\n`, writing via `FSWrite`, `clog_shutdown()` calling `FSClose` and clearing initialized (idempotent), `clog_set_file()` storing filename pointer (ignored if initialized), `clog_level_name()` same as POSIX
- [x] T008 [P] [US2] Verify 68k build: `mkdir -p build-68k && cd build-68k && cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake && make` — confirm libclog.a is produced
- [x] T009 [P] [US2] Verify PPC build: `mkdir -p build-ppc && cd build-ppc && cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake && make` — confirm libclog.a is produced

**Checkpoint**: Classic Mac implementation complete. All three platforms build.

---

## Phase 5: User Story 3 - Compile-Time Stripping (Priority: P3)

**Goal**: Developer can strip all logging or filter by level at compile time with zero overhead

**Independent Test**: Build with `-DCLOG_STRIP`, inspect binary for absence of clog_write calls and format strings

### Implementation for User Story 3

- [x] T010 [US3] Verify compile-time stripping on POSIX: build test_clog.c with `-DCLOG_STRIP`, run `nm` or `objdump` on the resulting binary to confirm no references to `clog_write`, run the binary to confirm no output is produced. Also build with `-DCLOG_MIN_LEVEL=2` and verify only ERR and WRN calls remain. Document verification results as a comment in CMakeLists.txt or in a build note

**Checkpoint**: Stripping verified — zero-overhead release builds confirmed.

---

## Phase 6: User Story 4 - Custom Output Destination (Priority: P4)

**Goal**: Developer can redirect log output to a named file on any platform

**Independent Test**: Set custom filename before init, log messages, verify output appears in the custom file

### Implementation for User Story 4

- [x] T011 [US4] Verify clog_set_file behavior on POSIX: extend tests/test_clog.c (or verify existing tests cover) that clog_set_file("custom.log") before init redirects output to custom.log, clog_set_file(NULL) reverts to stderr, clog_set_file after init has no effect. Run tests and confirm all pass

**Checkpoint**: Custom output destination verified on POSIX.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final validation and cleanup

- [x] T012 [P] Verify total line count across include/clog.h + src/clog_posix.c + src/clog_mac.c is under 500 lines (constitution principle VIII)
- [x] T013 Run quickstart.md validation: follow each step in specs/001-clog-library/quickstart.md on a fresh POSIX build and confirm all examples work as documented

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 (directory structure must exist)
- **US1 (Phase 3)**: Depends on Phase 2 (header must be complete)
- **US2 (Phase 4)**: Depends on Phase 2 (header must be complete). Independent of US1.
- **US3 (Phase 5)**: Depends on Phase 3 (needs POSIX build and test to verify against)
- **US4 (Phase 6)**: Depends on Phase 3 (needs POSIX implementation to test set_file)
- **Polish (Phase 7)**: Depends on Phases 3 + 4

### User Story Dependencies

- **US1 (P1)**: Depends on header (Phase 2). No dependency on other stories.
- **US2 (P2)**: Depends on header (Phase 2). No dependency on US1 (parallel-capable).
- **US3 (P3)**: Depends on US1 (needs compiled POSIX binary to verify stripping).
- **US4 (P4)**: Depends on US1 (needs POSIX implementation to test set_file behavior).

### Within Each User Story

- Implementation before verification
- POSIX implementation (US1) before stripping verification (US3)
- POSIX implementation (US1) before set_file verification (US4)

### Parallel Opportunities

- T008 and T009 (68k and PPC builds) can run in parallel
- T012 and T013 (polish tasks) can run in parallel
- US1 (Phase 3) and US2 (Phase 4) can run in parallel after Phase 2

---

## Parallel Example: Phase 4

```bash
# After clog_mac.c is written (T007), launch both cross-compile
# verifications in parallel:
Task T008: "Verify 68k build with Retro68 m68k toolchain"
Task T009: "Verify PPC build with Retro68 PPC toolchain"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T002)
2. Complete Phase 2: Header (T003)
3. Complete Phase 3: POSIX implementation + test (T004-T006)
4. **STOP and VALIDATE**: POSIX logging works end-to-end
5. Any C89 project can link on POSIX at this point

### Incremental Delivery

1. Setup + Header → Foundation ready
2. US1 (POSIX impl) → Test on POSIX → MVP!
3. US2 (Mac impl) → Cross-compile verify → All platforms build
4. US3 (Stripping) → Verify zero overhead → Release-ready
5. US4 (Custom file) → Verify set_file → Feature-complete
6. Polish → Line count + quickstart validation → Done

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- US1 and US2 can run in parallel (different .c files, same header)
- US3 and US4 are primarily verification — the implementation is
  already in clog.h (stripping macros) and clog_posix.c/clog_mac.c
  (set_file function)
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently

---

## Phase 8: Feedback Remediation (Consumer Integration Findings)

**Purpose**: Address findings from consumer integration testing

- [x] T014 [US1] Fix POSIX timestamp overflow in `src/clog_posix.c` `clog_write()`: handle microsecond borrow when `now.tv_usec < start_time.tv_usec` by subtracting 1 from seconds delta and adding 1000000 to microseconds delta before dividing. Add test case in `tests/test_clog.c` that sleeps >1 second and verifies timestamp is reasonable (not near UINT64_MAX). Verify with `cd build && cmake .. && make && ctest --output-on-failure`
- [x] T015 [P] [US1] Add `#pragma GCC diagnostic` suppression in `include/clog.h`: wrap the macro definition block (lines with `##__VA_ARGS__`) in `#pragma GCC diagnostic push` / `ignored "-Wvariadic-macros"` / `#pragma GCC diagnostic pop`, guarded by `#if defined(__GNUC__)`. Verify by building test with `-pedantic` flag added temporarily
- [x] T016 [US4] Change `clog_set_file()` return type from `void` to `int` in `include/clog.h`, `src/clog_posix.c`, and `src/clog_mac.c`: return 0 on success, -1 if already initialized. Update `specs/001-clog-library/contracts/clog-api.md` signature. Add test in `tests/test_clog.c` verifying return value is -1 when called after init
- [x] T017 [P] Add `option(CLOG_BUILD_TESTS "Build clog test applications" ON)` to `CMakeLists.txt` and wrap test section in `if(CLOG_BUILD_TESTS)`. Also add `if(NOT TARGET clog)` or `CMAKE_CURRENT_SOURCE_DIR` guard so `project()` is only called at top-level. Verify standalone build still works. Verify `add_subdirectory()` works by temporarily creating a test consumer CMakeLists.txt, building it, then removing the test files (do not commit test consumer files)
- [x] T018 [P] Add `__attribute__((format(printf, 2, 3)))` to `clog_write` declaration in `include/clog.h`, guarded by `#if defined(__GNUC__)`. Verify `CLOG_INFO("literal string")` compiles without -Wformat-security warning
- [x] T019 [P] [US2] Add `MaxApplZone()` call at the start of `clog_init()` in `src/clog_mac.c` (include `<Memory.h>`). Add comment documenting idempotent behavior. Verify 68k and PPC cross-builds still succeed
- [x] T020 [US4] Add `clog_set_append(int enable)` function: declare in `include/clog.h`, implement in both `src/clog_posix.c` (use `"a"` fopen mode) and `src/clog_mac.c` (skip `SetEOF` truncation, `SetFPos` to end). Add `append` field to static state struct. Must be called before init (same pattern as `clog_set_file`). Return `int` (0 success, -1 if already initialized). Add test in `tests/test_clog.c` verifying append mode preserves existing file content
- [x] T021 [P] Document memory footprint in `include/clog.h` header comment block and `README.md`: state struct ~50 bytes + static buffer (192B Mac, 256B POSIX), total ~250 bytes static on Mac, ~310 bytes on POSIX, zero heap allocation

**Checkpoint**: All consumer-reported issues addressed. Rebuild all three platforms.

### Phase 8 Dependencies

- T014: Independent — can start immediately
- T015: Independent — can start immediately
- T016: Independent — can start immediately
- T017: Independent — can start immediately
- T018: Independent — can start immediately
- T019: Independent — can start immediately (Mac-only, cross-compile verify)
- T020: Depends on T016 (set_append follows same return-int pattern as updated set_file)
- T021: Independent — can start immediately (documentation only)
- T014, T015, T016, T017, T018, T019, T021 can all run in parallel
