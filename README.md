# clog

Minimal C89 logging library for Classic Macintosh and modern systems.

## API

```c
clog_init("MyApp", CLOG_INFO);
CLOG_INFO("Connected to %s", peer_name);
CLOG_ERR("Send failed: %d", err);
clog_shutdown();
```

6 functions, 4 macros. Output: `[1234][INF] Connected to PlayerTwo`

## Build

```bash
# POSIX
mkdir -p build && cd build && cmake .. && make

# 68k (Retro68)
mkdir -p build-68k && cd build-68k
cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make

# PPC (Retro68)
mkdir -p build-ppc && cd build-ppc
cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
make
```

All targets produce `libclog.a`.

## Test

```bash
cd build && ctest --output-on-failure
```

## Usage

Link against `libclog.a` and add `include/` to your include path.

### Compile-time stripping

```
-DCLOG_STRIP          # Remove all logging (zero overhead)
-DCLOG_MIN_LEVEL=2    # Keep only ERR and WRN
```

### Custom output file

```c
clog_set_file("debug.log");   /* Call before clog_init */
clog_init("MyApp", CLOG_DEBUG);
```

POSIX defaults to stderr. Classic Mac defaults to a text file named after the app.

## Platforms

| Platform | Output | Timestamp |
|----------|--------|-----------|
| POSIX | stderr or file (stdio) | gettimeofday() ms delta |
| 68k Mac | File Manager text file | TickCount() ms delta |
| PPC Mac | File Manager text file | TickCount() ms delta |

## Memory Footprint

Static allocation only, zero heap usage:

| Platform | State struct | Format buffer | Total |
|----------|-------------|---------------|-------|
| POSIX | ~50 bytes | 256 bytes | ~310 bytes |
| Classic Mac | ~50 bytes | 192 bytes | ~250 bytes |

## Constraints

- C89/C90 strict
- No dynamic allocation
- Not interrupt-safe (log from main loop only)
- Under 500 lines total
