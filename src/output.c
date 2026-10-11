#define _POSIX_C_SOURCE 200809L

/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "output.h"

/* ========================================================================= */
/* COLOR STATE                                                               */
/* ========================================================================= */

#define RESET "\033[0m"

#define LABEL_ERROR "\033[1;31m"  /* bold red   */
#define LABEL_WARN "\033[1;33m"   /* bold yellow */
#define LABEL_HINT "\033[36m"     /* cyan       */
#define MARK_OK "\033[32m"        /* green      */

/* Environment facts, scanned once by out_init(). */
static int g_scanned = 0;
static int g_no_color = 0;    /* NO_COLOR set non-empty */
static int g_clicolor_off = 0; /* CLICOLOR=0            */
static int g_force = 0;       /* CLICOLOR_FORCE asks for color */
static int g_dumb_term = 0;   /* TERM=dumb             */

/* Programmatic override from out_set_color_mode(). */
static out_color_mode_t g_override = OUT_COLOR_AUTO;

static void scan_env(void)
{
    if (g_scanned)
    {
        return;
    }
    g_scanned = 1;

    /* Start from a clean slate: a rescan (out_init) must reflect the
     * current environment, not accumulate the previous one. */
    g_no_color = 0;
    g_clicolor_off = 0;
    g_force = 0;
    g_dumb_term = 0;

    const char *v;

    /* no-color.org: any value other than "0"... the reference site says the
     * variable's presence (non-empty) disables color; treat empty as unset
     * so `NO_COLOR=}` typos do not silently kill color. */
    v = getenv("NO_COLOR");
    if (v != NULL && v[0] != '\0')
    {
        g_no_color = 1;
    }

    v = getenv("CLICOLOR");
    if (v != NULL && strcmp(v, "0") == 0)
    {
        g_clicolor_off = 1;
    }

    v = getenv("CLICOLOR_FORCE");
    if (v != NULL && v[0] != '\0' && strcmp(v, "0") != 0)
    {
        g_force = 1;
    }

    v = getenv("TERM");
    if (v != NULL && strcmp(v, "dumb") == 0)
    {
        g_dumb_term = 1;
    }
}

void out_init(void)
{
    /* Explicit out_init() calls always rescan: main() calls it once, and
     * tests rely on being able to change the environment between cases. */
    g_scanned = 0;
    scan_env();
}

void out_set_color_mode(out_color_mode_t mode)
{
    g_override = mode;
}

int out_color_enabled(FILE *stream)
{
    scan_env();

    if (g_override != OUT_COLOR_AUTO)
    {
        return g_override == OUT_COLOR_ON;
    }

    /* An explicit NO_COLOR wins over CLICOLOR_FORCE: the user opted out of
     * color entirely, and that choice is the strongest signal. */
    if (g_no_color)
    {
        return 0;
    }
    if (g_force)
    {
        return 1;
    }
    if (g_clicolor_off || g_dumb_term)
    {
        return 0;
    }
    return isatty(fileno(stream)) == 1;
}

int out_stdout_is_tty(void)
{
    static int cached = -1;
    if (cached < 0)
    {
        cached = isatty(fileno(stdout)) == 1;
    }
    return cached;
}

int out_term_width(FILE *stream)
{
    struct winsize ws;
    if (ioctl(fileno(stream), TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
    {
        return (int)ws.ws_col;
    }

    const char *cols = getenv("COLUMNS");
    if (cols != NULL && cols[0] != '\0')
    {
        char *end = NULL;
        long v = strtol(cols, &end, 10);
        if (end != cols && *end == '\0' && v >= 20 && v <= 1000)
        {
            return (int)v;
        }
    }

    return 80;
}

/* ========================================================================= */
/* ESCAPE HELPERS                                                            */
/* ========================================================================= */

const char *out_sgr(FILE *stream, const char *code)
{
    if (!out_color_enabled(stream))
    {
        return "";
    }

    /* Ring of buffers so a single printf() can mix several results. */
    static char slots[16][24];
    static unsigned next = 0;
    char *s = slots[next++ % 16];
    snprintf(s, sizeof slots[0], "\033[%sm", code);
    return s;
}

const char *out_reset(FILE *stream)
{
    return out_sgr(stream, "0");
}

/* ========================================================================= */
/* MESSAGE HELPERS                                                           */
/* ========================================================================= */

/* Shared body: "vyt: " + optional colored "label: " + formatted message. */
static void vmsg(FILE *stream, const char *label, const char *label_sgr,
                 const char *fmt, va_list ap)
{
    scan_env();

    fputs("vyt: ", stream);
    if (label != NULL)
    {
        if (out_color_enabled(stream))
        {
            fprintf(stream, "%s%s%s", label_sgr, label, RESET);
        }
        else
        {
            fputs(label, stream);
        }
        fputs(": ", stream);
    }
    vfprintf(stream, fmt, ap);
    fputc('\n', stream);
}

void out_ok(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    scan_env();
    fputs("vyt: ", stream);
    if (out_color_enabled(stream))
    {
        fputs(MARK_OK "\u2713" RESET " ", stream);
    }
    else
    {
        fputs("\u2713 ", stream);
    }
    vfprintf(stream, fmt, ap);
    fputc('\n', stream);

    va_end(ap);
}

void out_status(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmsg(stream, NULL, NULL, fmt, ap);
    va_end(ap);
}

void out_error(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmsg(stream, "error", LABEL_ERROR, fmt, ap);
    va_end(ap);
}

void out_warn(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmsg(stream, "warning", LABEL_WARN, fmt, ap);
    va_end(ap);
}

void out_hint(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmsg(stream, "hint", LABEL_HINT, fmt, ap);
    va_end(ap);
}

/* ========================================================================= */
/* TEXT SHAPING                                                              */
/* ========================================================================= */

char *out_truncate_utf8(char *buf, size_t bufsz, const char *s,
                        size_t max_cols)
{
    size_t len = strlen(s);

    if (len <= max_cols || bufsz < 4)
    {
        if (len < bufsz)
        {
            memcpy(buf, s, len + 1);
            return buf;
        }
        /* Not even the untruncated string fits: fall through to a hard
         * truncation at the buffer size. */
    }

    /* Reserve one display column for the ellipsis (3 bytes, 1 column). */
    size_t keep = (max_cols > 0) ? max_cols - 1 : 0;
    if (keep > bufsz - 4)
    {
        keep = bufsz - 4;
    }

    /* Step back to a UTF-8 start byte so we never split a code point. */
    while (keep > 0 && ((unsigned char)s[keep] & 0xC0) == 0x80)
    {
        keep--;
    }

    memcpy(buf, s, keep);
    memcpy(buf + keep, "\u2026", 4); /* "…" + NUL */
    return buf;
}
