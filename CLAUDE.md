# clog

Minimal C89 logging library for Classic Macintosh and modern systems.

## Purpose

Provides `CLOG_ERR/WARN/INFO/DEBUG` macros that write timestamped messages to stderr (POSIX) or a text file (Classic Mac). Used by PeerTalk SDK and any other Classic Mac C project that needs logging.

## Design

- One header (`clog.h`), two implementations (`clog_posix.c`, `clog_mac.c`)
- 5 functions, 4 convenience macros
- C89 compatible, no dynamic allocation
- Compile-time stripping with `-DCLOG_STRIP`
- NOT interrupt-safe (log from main loop only)

## Build Environment

All builds run natively on the host (Ubuntu 25.10):

- **POSIX**: `cmake .. && make`
- **68k**: `cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake && make`
- **PPC**: `cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/powerpc-apple-macos/cmake/retro68.toolchain.cmake && make`

## File Structure

```
include/
  clog.h              # Public API
src/
  clog_posix.c        # POSIX: fprintf to stderr/file
  clog_mac.c          # Classic Mac: File Manager (FSOpen/FSWrite)
tests/
  test_clog.c         # Basic test
CMakeLists.txt        # Build config
```

## Relationship to PeerTalk

clog is a dependency of PeerTalk (`~/Desktop/peertalk`). PeerTalk includes `clog.h` in internal .c files (never in `peertalk.h`). Build clog first, then PeerTalk links against `libclog.a`.

## Target Size

Under 500 lines total. Header ~100 lines, each implementation ~150 lines.
