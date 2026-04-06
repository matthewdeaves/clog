/*
 * clog_mac.c — Classic Mac implementation (File Manager)
 *
 * Output: File Manager (Create/FSOpen/FSWrite/FSClose)
 * Timestamp: TickCount() delta from init, converted to ms
 */

#include "clog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <Files.h>
#include <Memory.h>
#include <Timer.h>

#define CLOG_BUF_SIZE 192

static struct {
    char              app_name[32];
    ClogLevel         min_level;
    int               initialized;
    unsigned long     start_ticks;
    short             ref_num;
    const char       *custom_file;
    int               append;
    ClogNetworkSink   net_sink;
    void             *net_sink_data;
} clog_state;

int clog_init(const char *app_name, ClogLevel level)
{
    OSErr err;
    const char *fname;
    Str255 pname;
    int len;

    if (app_name == NULL)
        return -1;

    /* Expand heap to maximum; idempotent if already called */
    MaxApplZone();

    strncpy(clog_state.app_name, app_name, 31);
    clog_state.app_name[31] = '\0';
    clog_state.min_level = level;

    fname = (clog_state.custom_file != NULL)
          ? clog_state.custom_file
          : clog_state.app_name;

    /* Convert C string to Pascal string */
    len = (int)strlen(fname);
    if (len > 255) len = 255;
    pname[0] = (unsigned char)len;
    memcpy(&pname[1], fname, (size_t)len);

    /* Create file (ignore error if exists) */
    err = Create(pname, 0, 'CLog', 'TEXT');
    if (err != noErr && err != dupFNErr)
        return -1;

    /* Open file */
    err = FSOpen(pname, 0, &clog_state.ref_num);
    if (err != noErr)
        return -1;

    if (clog_state.append) {
        /* Seek to end to preserve existing content */
        SetFPos(clog_state.ref_num, fsFromLEOF, 0);
    } else {
        /* Truncate to zero for fresh log */
        SetEOF(clog_state.ref_num, 0);
    }

    clog_state.start_ticks = TickCount();
    clog_state.initialized = 1;
    return 0;
}

void clog_shutdown(void)
{
    if (!clog_state.initialized)
        return;

    FSClose(clog_state.ref_num);
    clog_state.ref_num = 0;
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

void clog_set_network_sink(ClogNetworkSink fn, void *user_data)
{
    clog_state.net_sink = fn;
    clog_state.net_sink_data = user_data;
}

void clog_write(ClogLevel level, const char *fmt, ...)
{
    static char buf[CLOG_BUF_SIZE];
    static volatile int in_write = 0;
    unsigned long ms;
    const char *lvl;
    int prefix_len;
    int msg_len;
    long count;
    va_list ap;

    if (in_write)
        return;
    if (!clog_state.initialized)
        return;
    if (level > clog_state.min_level)
        return;

    in_write = 1;

    /* TickCount is 1/60th second; convert to ms: ticks * 1000 / 60 */
    ms = (TickCount() - clog_state.start_ticks) * 50UL / 3UL;

    lvl = clog_level_name(level);

    prefix_len = sprintf(buf, "[%lu][%s] ", ms, lvl);
    if (prefix_len < 0) {
        in_write = 0;
        return;
    }

    va_start(ap, fmt);
    vsprintf(buf + prefix_len, fmt, ap);
    va_end(ap);

    buf[CLOG_BUF_SIZE - 3] = '\0';  /* Leave room for \r\n */

    msg_len = (int)strlen(buf);

    /* Send to network sink before file write (survives crashes) */
    if (clog_state.net_sink) {
        clog_state.net_sink(buf, msg_len, clog_state.net_sink_data);
    }

    /* Append Mac line ending */
    buf[msg_len]     = '\r';
    buf[msg_len + 1] = '\n';
    msg_len += 2;

    /* Write immediately (no buffering) */
    count = (long)msg_len;
    SetFPos(clog_state.ref_num, fsFromLEOF, 0);
    FSWrite(clog_state.ref_num, &count, buf);

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
