# clog

Minimal C89 logging library for Classic Macintosh and modern systems.

## API

```c
clog_init("MyApp", CLOG_INFO);
CLOG_INFO("Connected to %s", peer_name);
CLOG_ERR("Send failed: %d", err);
clog_shutdown();
```

7 functions, 4 macros. Output: `[1234][INF] Connected to PlayerTwo`

## Prerequisites

System packages required: `gcc`, `cmake`, `make`.

- **Ubuntu/Debian:** `sudo apt install build-essential cmake`
- **macOS:** Install Xcode Command Line Tools (`xcode-select --install`) + `brew install cmake`

## Setup

```bash
./setup.sh
```

This configures everything needed to build clog. It also sets the `$CLOG_DIR` environment variable, which points to the clog project root and is used by downstream projects to locate clog.

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

### CMake integration (add_subdirectory)

```cmake
set(CLOG_BUILD_TESTS OFF)
add_subdirectory(path/to/clog)
target_link_libraries(myapp PRIVATE clog)
```

Includes propagate automatically via `PUBLIC` target property.

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

### Append mode

```c
clog_set_file("debug.log");
clog_set_append(1);           /* Preserve existing file content */
clog_init("MyApp", CLOG_DEBUG);
```

### Network sink

```c
void my_sink(const char *msg, int len, void *user_data) {
    send_over_network(msg, len);
}
clog_set_network_sink(my_sink, NULL);
```

Each formatted log line is delivered to the callback before being written to file. Useful for remote log collection from Classic Macs. Re-entrant calls from within the sink are suppressed.

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

## Dependency Chain

[Retro68](https://github.com/matthewdeaves/Retro68) (setup.sh) -> clog -> [peertalk](https://github.com/matthewdeaves/peertalk) -> [csend](https://github.com/matthewdeaves/csend)

clog itself is standalone, but Retro68 must be set up first for Classic Mac cross-compilation.

## Next Step

After setting up clog, set up [peertalk](https://github.com/matthewdeaves/peertalk).
