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

static void test_styles(void)
{
    out_set_color_mode(OUT_COLOR_OFF);
    char *s = capture(sink_status, "x");
    CHECK(strchr(s, '\033') == NULL, "style: off mode emits nothing");
    free(s);

    out_set_color_mode(OUT_COLOR_ON);
    CHECK_STREQ(out_style(stdout, OUT_BOLD), "\033[1m", "style: bold code");
    CHECK_STREQ(out_style(stdout, OUT_DIM), "\033[2m", "style: dim code");
    CHECK_STREQ(out_style(stdout, OUT_RESET), "\033[0m", "style: reset code");
    CHECK_STREQ(out_style(stdout, OUT_ERROR), "\033[1;31m", "style: error label");
    CHECK_STREQ(out_style(stdout, OUT_WARNING), "\033[1;33m",
                "style: warning label");
    CHECK_STREQ(out_style(stdout, OUT_HINT), "\033[36m", "style: hint label");
    CHECK_STREQ(out_style(stdout, OUT_OK), "\033[32m", "style: ok mark");

    /* Two different styles never share storage. */
    const char *a = out_style(stdout, OUT_BOLD);
    const char *b = out_style(stdout, OUT_DIM);
    CHECK(a != b, "style: different styles use different buffers");

    /* The same style may be used twice in one printf(): its text never
     * changes, so the two results are interchangeable. */
    const char *c = out_style(stdout, OUT_BOLD);
    CHECK_STREQ(c, a, "style: the same style always renders the same");

    out_set_color_mode(OUT_COLOR_AUTO);
}

/* ------------------------------------------------------------------------ */
/* layout helpers                                                            */
/* ------------------------------------------------------------------------ */

static void test_num_cols(void)
{
    CHECK(out_num_cols(1) == 1, "num_cols: one digit");
    CHECK(out_num_cols(9) == 1, "num_cols: nine is one column");
    CHECK(out_num_cols(10) == 2, "num_cols: ten needs two");
    CHECK(out_num_cols(99) == 2, "num_cols: ninety-nine needs two");
    CHECK(out_num_cols(100) == 3, "num_cols: a hundred needs three");
    CHECK(out_num_cols(1000) == 4, "num_cols: a thousand needs four");
}

static void test_fit_cols(void)
{
    /* The hard cap wins on a wide terminal. */
    CHECK(out_fit_cols(80, 0, 40) == 40, "fit: the hard cap wins");
    CHECK(out_fit_cols(80, 4, 40) == 40, "fit: the hard cap wins with an indent");

    /* A narrow terminal leaves less room, never more. */
    CHECK(out_fit_cols(20, 4, 40) == 15, "fit: a narrow terminal has less room");
    CHECK(out_fit_cols(12, 4, 40) == 7, "fit: the room shrinks with the indent");

    /* No room at all. */
    CHECK(out_fit_cols(5, 4, 40) == 0, "fit: a tiny terminal leaves no room");
    CHECK(out_fit_cols(0, 0, 40) == 0, "fit: no width means no room");
}

static void sink_dim(FILE *out, void *user)
{
    out_print_dim(out, 4, (const char *)user);
}

static void test_print_dim(void)
{
    out_set_color_mode(OUT_COLOR_OFF);
    char *s = capture(sink_dim, "https://youtu.be/abc");
    CHECK_STREQ(s, "    https://youtu.be/abc\n", "print_dim: indent and text");
    free(s);

    out_set_color_mode(OUT_COLOR_ON);
    s = capture(sink_dim, "x");
    CHECK_STREQ(s, "\033[2m    x\033[0m\n", "print_dim: dimmed when color is on");
    free(s);

    out_set_color_mode(OUT_COLOR_AUTO);
}

static void test_safe_line(void)
{
    char buf[64];

    out_safe_line(buf, sizeof buf, "plain text");
    CHECK_STREQ(buf, "plain text", "safe_line: plain text unchanged");

    /* Control characters become spaces: a query can never rearrange the
     * line it is printed on. */
    out_safe_line(buf, sizeof buf, "a\x1b[31mb");
    CHECK_STREQ(buf, "a [31mb", "safe_line: escape sequence neutralized");

    out_safe_line(buf, sizeof buf, "tab\there");
    CHECK_STREQ(buf, "tab here", "safe_line: tab neutralized");

    /* The result always fits the buffer. */
    char small[8];
    out_safe_line(small, sizeof small, "0123456789");
    CHECK(strlen(small) < sizeof small, "safe_line: respects the buffer size");
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
    test_styles();
    test_num_cols();
    test_fit_cols();
    test_print_dim();
    test_safe_line();
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
