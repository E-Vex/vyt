#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/output.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg)                                                    \
    do                                                                      \
    {                                                                       \
        checks++;                                                           \
        if (!(cond))                                                        \
        {                                                                   \
            failures++;                                                     \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        }                                                                   \
    } while (0)

#define CHECK_STREQ(got, want, msg)                                         \
    do                                                                      \
    {                                                                       \
        const char *g_ = (got);                                             \
        const char *w_ = (want);                                            \
        checks++;                                                           \
        if (g_ == NULL || strcmp(g_, w_) != 0)                              \
        {                                                                   \
            failures++;                                                     \
            fprintf(stderr, "FAIL: %s (got \"%s\", want \"%s\") (%s:%d)\n", \
                    msg, g_ ? g_ : "(null)", w_, __FILE__, __LINE__);       \
        }                                                                   \
    } while (0)

/* Runs `fn` with a memstream; returns the collected text (caller frees)
 * and guarantees fclose() happened before the string is compared. */
typedef void (*sink_fn)(FILE *out, void *user);

static char *capture(sink_fn fn, void *user)
{
    char *buf = NULL;
    size_t len = 0;
    FILE *ms = open_memstream(&buf, &len);
    if (ms == NULL)
    {
        return NULL;
    }
    fn(ms, user);
    fclose(ms);
    return buf;
}

/* ------------------------------------------------------------------------ */
/* truncation                                                                */
/* ------------------------------------------------------------------------ */

static void test_truncate(void)
{
    char buf[64];

    /* Short strings pass through untouched. */
    CHECK_STREQ(out_truncate_utf8(buf, sizeof buf, "hello", 10), "hello",
                "truncate: short string unchanged");
    CHECK_STREQ(out_truncate_utf8(buf, sizeof buf, "exactly-10!", 11),
                "exactly-10!", "truncate: exact fit unchanged");

    /* ASCII truncation reserves one column for the ellipsis. */
    CHECK_STREQ(out_truncate_utf8(buf, sizeof buf, "abcdefgh", 5), "abcd\u2026",
                "truncate: ascii truncated with ellipsis");

    /* A multi-byte string is cut on a code-point boundary.  Byte length is
     * the column approximation: max_cols 3 keeps at most 3 bytes, and
     * "éé" is 4 bytes, so only "é" fits before the ellipsis. */
    CHECK_STREQ(out_truncate_utf8(buf, sizeof buf, "\u00e9\u00e9\u00e9\u00e9", 4),
                "\u00e9\u2026", "truncate: utf-8 boundary respected");

    /* Truncating at 1 column yields just the ellipsis. */
    CHECK_STREQ(out_truncate_utf8(buf, sizeof buf, "abc", 1), "\u2026",
                "truncate: max_cols 1 -> ellipsis only");

    /* A tight buffer still produces a NUL-terminated string.  bufsz 8
     * leaves room for 4 content bytes + ellipsis + NUL. */
    char tiny[8];
    char *r = out_truncate_utf8(tiny, sizeof tiny, "abcdefghijkl", 6);
    CHECK(r != NULL && strlen(r) < sizeof tiny, "truncate: respects bufsz");
    CHECK_STREQ(r, "abcd\u2026", "truncate: tight buffer keeps ellipsis");
}

/* ------------------------------------------------------------------------ */
/* escapes and message shapes                                                */
/* ------------------------------------------------------------------------ */

static void sink_ok(FILE *out, void *user)
{
    out_ok(out, "created playlist '%s'", (const char *)user);
}

static void sink_status(FILE *out, void *user)
{
    out_status(out, "%s", (const char *)user);
}

static void sink_error(FILE *out, void *user)
{
    out_error(out, "%s", (const char *)user);
}

static void sink_warn(FILE *out, void *user)
{
    out_warn(out, "%s", (const char *)user);
}

static void sink_hint(FILE *out, void *user)
{
    out_hint(out, "%s", (const char *)user);
}

static void test_message_shapes_color_off(void)
{
    out_set_color_mode(OUT_COLOR_OFF);

    char *s = capture(sink_ok, "chill");
    CHECK_STREQ(s, "vyt: \u2713 created playlist 'chill'\n",
                "ok: shape without color");
    free(s);

    s = capture(sink_status, "no playlists yet");
    CHECK_STREQ(s, "vyt: no playlists yet\n", "status: shape without color");
    free(s);

    s = capture(sink_error, "boom");
    CHECK_STREQ(s, "vyt: error: boom\n", "error: shape without color");
    free(s);

    s = capture(sink_warn, "careful");
    CHECK_STREQ(s, "vyt: warning: careful\n", "warn: shape without color");
    free(s);

    s = capture(sink_hint, "try this");
    CHECK_STREQ(s, "vyt: hint: try this\n", "hint: shape without color");
    free(s);

    /* No escapes anywhere. */
    s = capture(sink_error, "x");
    CHECK(strchr(s, '\033') == NULL, "error: no escapes when color off");
    free(s);
}

static void test_message_shapes_color_on(void)
{
    out_set_color_mode(OUT_COLOR_ON);

    char *s = capture(sink_ok, "chill");
    CHECK_STREQ(s, "vyt: \033[32m\u2713\033[0m created playlist 'chill'\n",
                "ok: green check when color on");
    free(s);

    s = capture(sink_error, "boom");
    CHECK_STREQ(s, "vyt: \033[1;31merror\033[0m: boom\n",
                "error: bold-red label when color on");
    free(s);

    s = capture(sink_warn, "careful");
    CHECK_STREQ(s, "vyt: \033[1;33mwarning\033[0m: careful\n",
                "warn: bold-yellow label when color on");
    free(s);

    s = capture(sink_hint, "try this");
    CHECK_STREQ(s, "vyt: \033[36mhint\033[0m: try this\n",
                "hint: cyan label when color on");
    free(s);

    out_set_color_mode(OUT_COLOR_AUTO);
}

static void test_sgr_composition(void)
{
    out_set_color_mode(OUT_COLOR_OFF);
    char *s = capture(sink_status, "x");
    CHECK(strchr(s, '\033') == NULL, "sgr: off mode emits nothing");
    free(s);

    out_set_color_mode(OUT_COLOR_ON);
    CHECK_STREQ(out_sgr(stdout, "1"), "\033[1m", "sgr: bold code");
    CHECK_STREQ(out_reset(stdout), "\033[0m", "sgr: reset code");

    /* Several distinct results must coexist inside one printf. */
    const char *a = out_sgr(stdout, "2");
    const char *b = out_sgr(stdout, "1;36");
    const char *c = out_reset(stdout);
    CHECK(a != b && b != c && strcmp(a, b) != 0,
          "sgr: consecutive calls return distinct buffers");
    CHECK_STREQ(a, "\033[2m", "sgr: first ring slot intact");

    out_set_color_mode(OUT_COLOR_AUTO);
}

/* ------------------------------------------------------------------------ */
/* color-mode precedence                                                     */
/* ------------------------------------------------------------------------ */

static void sink_probe(FILE *out, void *user)
{
    (void)user;
    fputs(out_color_enabled(out) ? "COLOR" : "PLAIN", out);
}

static void test_precedence(void)
{
    /* NO_COLOR beats CLICOLOR_FORCE. */
    setenv("NO_COLOR", "1", 1);
    setenv("CLICOLOR_FORCE", "1", 1);
    unsetenv("CLICOLOR");
    unsetenv("TERM");
    out_init();
    char *s = capture(sink_probe, NULL);
    CHECK_STREQ(s, "PLAIN", "precedence: NO_COLOR beats CLICOLOR_FORCE");
    free(s);

    /* CLICOLOR_FORCE wins over non-tty when NO_COLOR is absent. */
    unsetenv("NO_COLOR");
    out_init();
    s = capture(sink_probe, NULL);
    CHECK_STREQ(s, "COLOR", "precedence: CLICOLOR_FORCE forces color");
    free(s);

    /* CLICOLOR=0 disables on a non-tty (already off) AND the programmatic
     * override beats every environment setting. */
    setenv("CLICOLOR", "0", 1);
    unsetenv("CLICOLOR_FORCE");
    out_init();
    out_set_color_mode(OUT_COLOR_ON);
    s = capture(sink_probe, NULL);
    CHECK_STREQ(s, "COLOR", "precedence: override ON beats CLICOLOR=0");
    free(s);
    out_set_color_mode(OUT_COLOR_OFF);
    setenv("CLICOLOR_FORCE", "1", 1);
    out_init();
    s = capture(sink_probe, NULL);
    CHECK_STREQ(s, "PLAIN", "precedence: override OFF beats CLICOLOR_FORCE");
    free(s);

    /* Restore: tests must run colorless by default. */
    unsetenv("NO_COLOR");
    unsetenv("CLICOLOR");
    unsetenv("CLICOLOR_FORCE");
    unsetenv("TERM");
    out_set_color_mode(OUT_COLOR_AUTO);
    out_init();
    s = capture(sink_probe, NULL);
    CHECK_STREQ(s, "PLAIN",
                "precedence: plain on a non-tty with no env overrides");
    free(s);
}

/* ------------------------------------------------------------------------ */
/* terminal width                                                            */
/* ------------------------------------------------------------------------ */

static void test_term_width(void)
{
    int w = out_term_width(stdout);
    CHECK(w >= 20 && w <= 1000, "width: sane fallback value");

    setenv("COLUMNS", "123", 1);
    w = out_term_width(stdout);
    CHECK(w == 80 || w == 123,
          "width: COLUMNS respected on non-tty (or ioctl absent)");
    unsetenv("COLUMNS");
}

int main(void)
{
    /* Deterministic baseline: no inherited color knobs. */
    unsetenv("NO_COLOR");
    unsetenv("CLICOLOR");
    unsetenv("CLICOLOR_FORCE");
    unsetenv("TERM");
    unsetenv("COLUMNS");
    out_init();
    out_set_color_mode(OUT_COLOR_AUTO);

    test_truncate();
    test_message_shapes_color_off();
    test_message_shapes_color_on();
    test_sgr_composition();
    test_precedence();
    test_term_width();

    if (failures == 0)
    {
        printf("All %d checks passed.\n", checks);
        return 0;
    }

    fprintf(stderr, "%d/%d checks failed.\n", failures, checks);
    return 1;
}
