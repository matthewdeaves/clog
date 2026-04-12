/*
 * clog_posix.c — POSIX implementation (stdio)
 *
 * Output: fprintf to stderr (default) or file
 * Timestamp: gettimeofday() delta from init
 */

/* POSIX feature test for vsnprintf and gettimeofday */
#define _POSIX_C_SOURCE 200112L

#include "clog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define CLOG_BUF_SIZE 256

static struct {
    char              app_name[32];
    ClogLevel         min_level;
    int               initialized;
    struct timeval     start_time;
    FILE              *fp;
    const char        *custom_file;
    int               append;
    int               flush_mode;
    ClogNetworkSink   net_sink;
    void             *net_sink_data;
} clog_state;

int clog_init(const char *app_name, ClogLevel level)
{
    if (app_name == NULL)
        return -1;

    strncpy(clog_state.app_name, app_name, 31);
    clog_state.app_name[31] = '\0';
    clog_state.min_level = level;

    gettimeofday(&clog_state.start_time, NULL);

    if (clog_state.custom_file != NULL) {
        clog_state.fp = fopen(clog_state.custom_file,
                              clog_state.append ? "a" : "w");
        if (clog_state.fp == NULL)
            return -1;
    } else {
        clog_state.fp = stderr;
    }

    clog_state.initialized = 1;
    return 0;
}

void clog_shutdown(void)
{
    if (!clog_state.initialized)
        return;

    if (clog_state.fp != NULL && clog_state.fp != stderr)
        fclose(clog_state.fp);

    clog_state.fp = NULL;
    clog_state.initialized = 0;
}

int clog_set_append(int enable)
{
    if (clog_state.initialized)
        return -1;
    clog_state.append = enable;
    return 0;
}

void clog_set_level(ClogLevel level)
{
    clog_state.min_level = level;
}

int clog_set_file(const char *filename)
{
    if (clog_state.initialized)
        return -1;
    clog_state.custom_file = filename;
    return 0;
}

int clog_set_flush(int mode)
{
    if (clog_state.initialized)
        return -1;
    clog_state.flush_mode = mode;
    return 0;
}

void clog_set_network_sink(ClogNetworkSink fn, void *user_data)
{
    clog_state.net_sink = fn;
    clog_state.net_sink_data = user_data;
}

void clog_write(ClogLevel level, const char *fmt, ...)
{
    static char buf[CLOG_BUF_SIZE];
    static volatile int in_write = 0;
    struct timeval now;
    unsigned long ms;
    const char *lvl;
    int prefix_len;
    int msg_len;
    va_list ap;

    if (in_write)
        return;
    if (!clog_state.initialized)
        return;
    if (level > clog_state.min_level)
        return;

    in_write = 1;

    gettimeofday(&now, NULL);
    {
        long sec_delta = now.tv_sec - clog_state.start_time.tv_sec;
        long usec_delta = now.tv_usec - clog_state.start_time.tv_usec;
        if (usec_delta < 0) {
            sec_delta -= 1;
            usec_delta += 1000000;
        }
        ms = (unsigned long)sec_delta * 1000
           + (unsigned long)usec_delta / 1000;
    }

    lvl = clog_level_name(level);

    prefix_len = sprintf(buf, "[%lu][%s] ", ms, lvl);
    if (prefix_len < 0) {
        in_write = 0;
        return;
    }

    va_start(ap, fmt);
    {
        int body_len;
        int avail = CLOG_BUF_SIZE - prefix_len;
        body_len = vsnprintf(buf + prefix_len, (size_t)avail, fmt, ap);
        /* vsnprintf returns chars that would be written; cap to buffer */
        if (body_len < 0) body_len = 0;
        if (body_len >= avail) body_len = avail - 1;
        msg_len = prefix_len + body_len;
    }
    va_end(ap);

    buf[msg_len] = '\0';

    /* Send to network sink before file write (survives crashes) */
    if (clog_state.net_sink) {
        clog_state.net_sink(buf, msg_len, clog_state.net_sink_data);
    }

    fprintf(clog_state.fp, "%s\n", buf);
    fflush(clog_state.fp);

    /* fsync forces kernel buffers to disk if requested.  fflush above
     * only pushes stdio buffers to the kernel — data can still be lost
     * on OS crash without fsync. */
    if (clog_state.flush_mode == CLOG_FLUSH_ALL ||
        (clog_state.flush_mode == CLOG_FLUSH_ERRORS &&
         level <= CLOG_LVL_WARN)) {
        if (clog_state.fp != stderr) {
            fsync(fileno(clog_state.fp));
        }
    }

    in_write = 0;
}

const char *clog_level_name(ClogLevel level)
{
    switch (level) {
    case CLOG_LVL_ERR:  return "ERR";
    case CLOG_LVL_WARN: return "WRN";
    case CLOG_LVL_INFO: return "INF";
    case CLOG_LVL_DBG:  return "DBG";
    default:            return "???";
    }
}
