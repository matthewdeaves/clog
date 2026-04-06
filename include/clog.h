/*
 * clog.h — Minimal C89 logging library
 *
 * One header, two implementations:
 *   clog_posix.c  — stderr/file output via stdio
 *   clog_mac.c    — Classic Mac File Manager output
 *
 * Usage:
 *   clog_init("MyApp", CLOG_INFO);
 *   CLOG_INFO("Connected to %s", peer_name);
 *   clog_shutdown();
 *
 * NOT interrupt-safe. Never call from ASR, OT notifier,
 * or any interrupt context. Set a flag, log from main loop.
 *
 * Memory footprint (static, zero heap allocation):
 *   State struct ~50 bytes + format buffer (256B POSIX, 192B Mac)
 *   Total: ~310 bytes POSIX, ~250 bytes Classic Mac
 *
 * Compile-time control:
 *   -DCLOG_STRIP          Remove all logging (zero overhead)
 *   -DCLOG_MIN_LEVEL=N    Keep only levels <= N
 */

#ifndef CLOG_H
#define CLOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Log severity levels (lower value = higher severity) */
typedef enum {
    CLOG_LVL_ERR  = 1,
    CLOG_LVL_WARN = 2,
    CLOG_LVL_INFO = 3,
    CLOG_LVL_DBG  = 4
} ClogLevel;

/* Lifecycle */
int         clog_init(const char *app_name, ClogLevel level);
void        clog_shutdown(void);

/* Output control (call before clog_init, or ignored) */
int         clog_set_file(const char *filename);
int         clog_set_append(int enable);

/* Runtime level control */
void        clog_set_level(ClogLevel level);

/* Network sink — callback receives each formatted log line */
typedef void (*ClogNetworkSink)(const char *msg, int len, void *user_data);
void        clog_set_network_sink(ClogNetworkSink fn, void *user_data);

/* Logging */
#if defined(__GNUC__)
void        clog_write(ClogLevel level, const char *fmt, ...)
                __attribute__((format(printf, 2, 3)));
#else
void        clog_write(ClogLevel level, const char *fmt, ...);
#endif

/* Utility */
const char *clog_level_name(ClogLevel level);

/*
 * Convenience macros
 *
 * CLOG_STRIP        — all macros expand to ((void)0)
 * CLOG_MIN_LEVEL=N  — macros for levels > N expand to ((void)0)
 * CLOG_STRIP takes precedence over CLOG_MIN_LEVEL.
 */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvariadic-macros"
#endif

#ifdef CLOG_STRIP

#define CLOG_ERR(fmt, ...)   ((void)0)
#define CLOG_WARN(fmt, ...)  ((void)0)
#define CLOG_INFO(fmt, ...)  ((void)0)
#define CLOG_DEBUG(fmt, ...) ((void)0)

#elif defined(CLOG_MIN_LEVEL)

#if CLOG_MIN_LEVEL >= 1
#define CLOG_ERR(fmt, ...)   clog_write(CLOG_LVL_ERR, fmt, ##__VA_ARGS__)
#else
#define CLOG_ERR(fmt, ...)   ((void)0)
#endif

#if CLOG_MIN_LEVEL >= 2
#define CLOG_WARN(fmt, ...)  clog_write(CLOG_LVL_WARN, fmt, ##__VA_ARGS__)
#else
#define CLOG_WARN(fmt, ...)  ((void)0)
#endif

#if CLOG_MIN_LEVEL >= 3
#define CLOG_INFO(fmt, ...)  clog_write(CLOG_LVL_INFO, fmt, ##__VA_ARGS__)
#else
#define CLOG_INFO(fmt, ...)  ((void)0)
#endif

#if CLOG_MIN_LEVEL >= 4
#define CLOG_DEBUG(fmt, ...) clog_write(CLOG_LVL_DBG, fmt, ##__VA_ARGS__)
#else
#define CLOG_DEBUG(fmt, ...) ((void)0)
#endif

#else /* No stripping — all macros active */

#define CLOG_ERR(fmt, ...)   clog_write(CLOG_LVL_ERR, fmt, ##__VA_ARGS__)
#define CLOG_WARN(fmt, ...)  clog_write(CLOG_LVL_WARN, fmt, ##__VA_ARGS__)
#define CLOG_INFO(fmt, ...)  clog_write(CLOG_LVL_INFO, fmt, ##__VA_ARGS__)
#define CLOG_DEBUG(fmt, ...) clog_write(CLOG_LVL_DBG, fmt, ##__VA_ARGS__)

#endif /* CLOG_STRIP / CLOG_MIN_LEVEL */

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* CLOG_H */
