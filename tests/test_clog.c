/*
 * test_clog.c — Basic tests for clog POSIX implementation
 *
 * Returns 0 on success, 1 on any failure.
 */

#include "clog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;

static void check(int condition, const char *name)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    } else {
        fprintf(stderr, "PASS: %s\n", name);
    }
}

/* Read entire file into static buffer, return length */
static int read_file(const char *path, char *buf, int bufsize)
{
    FILE *f = fopen(path, "r");
    int n;
    if (f == NULL) return -1;
    n = (int)fread(buf, 1, (size_t)(bufsize - 1), f);
    fclose(f);
    buf[n] = '\0';
    return n;
}

static void test_init_success(void)
{
    int rc;
    clog_set_file("test_output.log");
    rc = clog_init("TestApp", CLOG_LVL_INFO);
    check(rc == 0, "init returns 0 on success");
    clog_shutdown();
    remove("test_output.log");
}

static void test_output_format(void)
{
    char buf[2048];
    int len;

    clog_set_file("test_format.log");
    clog_init("TestApp", CLOG_LVL_DBG);

    CLOG_ERR("error message %d", 42);
    CLOG_WARN("warn message");
    CLOG_INFO("info message");
    CLOG_DEBUG("debug message");

    clog_shutdown();

    len = read_file("test_format.log", buf, (int)sizeof(buf));
    check(len > 0, "output file is not empty");

    /* Verify format: [<digits>][LVL] message */
    check(strstr(buf, "][ERR] error message 42") != NULL,
          "ERR line has correct format");
    check(strstr(buf, "][WRN] warn message") != NULL,
          "WRN line has correct format");
    check(strstr(buf, "][INF] info message") != NULL,
          "INF line has correct format");
    check(strstr(buf, "][DBG] debug message") != NULL,
          "DBG line has correct format");

    /* Verify timestamp prefix starts with [ */
    check(buf[0] == '[', "output starts with timestamp bracket");

    remove("test_format.log");
}

static void test_level_filtering(void)
{
    char buf[2048];
    int len;

    clog_set_file("test_filter.log");
    clog_init("TestApp", CLOG_LVL_WARN);

    CLOG_ERR("visible error");
    CLOG_WARN("visible warn");
    CLOG_INFO("invisible info");
    CLOG_DEBUG("invisible debug");

    clog_shutdown();

    len = read_file("test_filter.log", buf, (int)sizeof(buf));
    check(len > 0, "filtered output file is not empty");
    check(strstr(buf, "visible error") != NULL,
          "ERR passes WARN filter");
    check(strstr(buf, "visible warn") != NULL,
          "WRN passes WARN filter");
    check(strstr(buf, "invisible info") == NULL,
          "INFO filtered by WARN level");
    check(strstr(buf, "invisible debug") == NULL,
          "DEBUG filtered by WARN level");

    remove("test_filter.log");
}

static void test_double_shutdown(void)
{
    clog_set_file("test_double.log");
    clog_init("TestApp", CLOG_LVL_INFO);
    clog_shutdown();
    clog_shutdown();
    check(1, "double shutdown does not crash");
    remove("test_double.log");
}

static void test_write_before_init(void)
{
    /* Should be a no-op, not crash */
    CLOG_INFO("should not crash");
    check(1, "write before init is a no-op");
}

static void test_shutdown_before_init(void)
{
    clog_shutdown();
    check(1, "shutdown before init is a no-op");
}

static void test_level_name(void)
{
    check(strcmp(clog_level_name(CLOG_LVL_ERR), "ERR") == 0,
          "level_name ERR");
    check(strcmp(clog_level_name(CLOG_LVL_WARN), "WRN") == 0,
          "level_name WRN");
    check(strcmp(clog_level_name(CLOG_LVL_INFO), "INF") == 0,
          "level_name INF");
    check(strcmp(clog_level_name(CLOG_LVL_DBG), "DBG") == 0,
          "level_name DBG");
    check(strcmp(clog_level_name((ClogLevel)99), "???") == 0,
          "level_name invalid returns ???");
}

static void test_custom_file(void)
{
    char buf[2048];
    int len;

    clog_set_file("custom_output.log");
    clog_init("TestApp", CLOG_LVL_INFO);
    CLOG_INFO("custom file test");
    clog_shutdown();

    len = read_file("custom_output.log", buf, (int)sizeof(buf));
    check(len > 0, "custom file has output");
    check(strstr(buf, "custom file test") != NULL,
          "custom file contains logged message");

    remove("custom_output.log");
}

static void test_set_file_after_init(void)
{
    char buf[2048];
    int len;
    int rc;

    clog_set_file("before_init.log");
    clog_init("TestApp", CLOG_LVL_INFO);

    /* set_file after init should return -1 */
    rc = clog_set_file("after_init.log");
    check(rc == -1, "set_file after init returns -1");
    CLOG_INFO("goes to before_init.log");
    clog_shutdown();

    len = read_file("before_init.log", buf, (int)sizeof(buf));
    check(len > 0, "set_file before init takes effect");
    check(strstr(buf, "goes to before_init.log") != NULL,
          "message in correct file");

    /* after_init.log should not exist */
    {
        FILE *f = fopen("after_init.log", "r");
        check(f == NULL, "set_file after init has no effect");
        if (f != NULL) fclose(f);
    }

    remove("before_init.log");
    remove("after_init.log");
}

static void test_set_file_null_revert(void)
{
    /* Set a custom file, then revert to NULL */
    clog_set_file("temp.log");
    clog_set_file(NULL);

    /* Init should use stderr (default) — we just verify no crash */
    clog_init("TestApp", CLOG_LVL_INFO);
    CLOG_INFO("to stderr");
    clog_shutdown();
    check(1, "set_file(NULL) reverts to default without crash");
    remove("temp.log");
}

static void test_set_append(void)
{
    char buf[2048];
    int len;
    FILE *f;

    /* First: write some content to the file */
    f = fopen("append_test.log", "w");
    if (f != NULL) {
        fprintf(f, "existing content\n");
        fclose(f);
    }

    /* Init with append mode */
    clog_set_file("append_test.log");
    clog_set_append(1);
    clog_init("TestApp", CLOG_LVL_INFO);
    CLOG_INFO("appended line");
    clog_shutdown();

    len = read_file("append_test.log", buf, (int)sizeof(buf));
    check(len > 0, "append test file not empty");
    check(strstr(buf, "existing content") != NULL,
          "append mode preserves existing content");
    check(strstr(buf, "appended line") != NULL,
          "append mode adds new content");

    /* Reset append flag for other tests */
    clog_set_append(0);
    remove("append_test.log");
}

static void test_set_append_after_init(void)
{
    int rc;
    clog_set_file("append_init.log");
    clog_init("TestApp", CLOG_LVL_INFO);
    rc = clog_set_append(1);
    check(rc == -1, "set_append after init returns -1");
    clog_shutdown();
    clog_set_append(0);
    remove("append_init.log");
}

static void test_set_flush_modes(void)
{
    char buf[2048];
    int len;
    int rc;

    /* CLOG_FLUSH_ALL */
    clog_set_file("test_flush.log");
    clog_set_flush(CLOG_FLUSH_ALL);
    clog_init("TestApp", CLOG_LVL_INFO);
    CLOG_ERR("flush err");
    CLOG_INFO("flush info");
    clog_shutdown();

    len = read_file("test_flush.log", buf, (int)sizeof(buf));
    check(len > 0, "flush_all output not empty");
    check(strstr(buf, "flush err") != NULL, "flush_all ERR present");
    check(strstr(buf, "flush info") != NULL, "flush_all INFO present");
    remove("test_flush.log");

    /* CLOG_FLUSH_ERRORS */
    clog_set_file("test_flush2.log");
    clog_set_flush(CLOG_FLUSH_ERRORS);
    clog_init("TestApp", CLOG_LVL_INFO);
    CLOG_ERR("ferr");
    CLOG_WARN("fwrn");
    CLOG_INFO("finf");
    clog_shutdown();

    len = read_file("test_flush2.log", buf, (int)sizeof(buf));
    check(len > 0, "flush_errors output not empty");
    check(strstr(buf, "ferr") != NULL, "flush_errors ERR present");
    check(strstr(buf, "fwrn") != NULL, "flush_errors WRN present");
    check(strstr(buf, "finf") != NULL, "flush_errors INFO present");
    remove("test_flush2.log");

    /* set_flush after init returns -1 */
    clog_set_file("test_flush3.log");
    clog_set_flush(CLOG_FLUSH_NONE);
    clog_init("TestApp", CLOG_LVL_INFO);
    rc = clog_set_flush(CLOG_FLUSH_ALL);
    check(rc == -1, "set_flush after init returns -1");
    clog_shutdown();
    remove("test_flush3.log");
}

static void test_timestamp_after_sleep(void)
{
    char buf[2048];
    int len;
    unsigned long ms;
    char *bracket;

    clog_set_file("test_sleep.log");
    clog_init("TestApp", CLOG_LVL_INFO);

    /* Sleep >1 second to trigger microsecond borrow scenario */
    sleep(1);
    CLOG_INFO("after sleep");
    clog_shutdown();

    len = read_file("test_sleep.log", buf, (int)sizeof(buf));
    check(len > 0, "sleep test output file not empty");

    /* Parse timestamp from [<ms>][INF] after sleep */
    bracket = strchr(buf, '[');
    if (bracket != NULL) {
        ms = strtoul(bracket + 1, NULL, 10);
        check(ms >= 900 && ms < 30000,
              "timestamp after 1s sleep is reasonable (not overflow)");
    } else {
        check(0, "timestamp after 1s sleep is reasonable (no bracket found)");
    }

    remove("test_sleep.log");
}

static int sink_called = 0;
static char sink_buf[512];
static int sink_len = 0;

static void test_sink(const char *msg, int len, void *user_data)
{
    (void)user_data;
    sink_called = 1;
    if (len > (int)sizeof(sink_buf) - 1)
        len = (int)sizeof(sink_buf) - 1;
    memcpy(sink_buf, msg, (size_t)len);
    sink_buf[len] = '\0';
    sink_len = len;
}

static void test_network_sink(void)
{
    sink_called = 0;
    sink_buf[0] = '\0';
    sink_len = 0;

    clog_set_file("test_sink.log");
    clog_set_network_sink(test_sink, NULL);
    clog_init("TestApp", CLOG_LVL_INFO);

    CLOG_ERR("sink test %d", 7);

    check(sink_called == 1, "network sink was called");
    check(sink_len > 0, "network sink received non-empty message");
    check(strstr(sink_buf, "sink test 7") != NULL,
          "network sink received formatted message");

    clog_shutdown();
    clog_set_network_sink(NULL, NULL);
    remove("test_sink.log");
}

static void test_runtime_level_change(void)
{
    char buf[2048];
    int len;

    clog_set_file("test_level_change.log");
    clog_init("TestApp", CLOG_LVL_INFO);

    CLOG_DEBUG("should not appear");

    /* Widen level to include DEBUG */
    clog_set_level(CLOG_LVL_DBG);
    CLOG_DEBUG("should appear");

    /* Narrow level to ERR only */
    clog_set_level(CLOG_LVL_ERR);
    CLOG_INFO("should not appear either");
    CLOG_ERR("err visible");

    clog_shutdown();

    len = read_file("test_level_change.log", buf, (int)sizeof(buf));
    check(len > 0, "level change output not empty");
    check(strstr(buf, "should not appear") == NULL,
          "DEBUG filtered before level change");
    check(strstr(buf, "should appear") != NULL,
          "DEBUG visible after widening to DBG");
    check(strstr(buf, "should not appear either") == NULL,
          "INFO filtered after narrowing to ERR");
    check(strstr(buf, "err visible") != NULL,
          "ERR visible after narrowing to ERR");

    remove("test_level_change.log");
}

static void test_double_init(void)
{
    char buf[2048];
    int len;
    int rc;

    /* First init writes to a file */
    clog_set_file("test_double_init.log");
    clog_init("TestApp", CLOG_LVL_INFO);
    CLOG_INFO("first init");

    /* Second init without explicit shutdown — should close old handle.
     * clog_init calls clog_shutdown internally, which re-opens the path
     * for set_file.  The same custom_file pointer persists. */
    rc = clog_init("TestApp2", CLOG_LVL_INFO);
    check(rc == 0, "double init succeeds");
    CLOG_INFO("second init");
    clog_shutdown();

    /* Both messages go to the same file (custom_file persists) */
    len = read_file("test_double_init.log", buf, (int)sizeof(buf));
    check(len > 0, "double init file has output");
    check(strstr(buf, "second init") != NULL,
          "second init writes after implicit shutdown");

    remove("test_double_init.log");
}

int main(void)
{
    test_init_success();
    test_output_format();
    test_level_filtering();
    test_double_shutdown();
    test_write_before_init();
    test_shutdown_before_init();
    test_level_name();
    test_custom_file();
    test_set_file_after_init();
    test_set_file_null_revert();
    test_set_append();
    test_set_append_after_init();
    test_set_flush_modes();
    test_timestamp_after_sleep();
    test_network_sink();
    test_runtime_level_change();
    test_double_init();

    fprintf(stderr, "\n%d failure(s)\n", failures);
    return failures > 0 ? 1 : 0;
}
