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

/*
 * The SGR code behind every named style.  A style always renders the same
 * way, which is what makes out_style() safe to call several times for the
 * same style inside one printf().
 */
static const char *const style_codes[] = {
    [OUT_BOLD] = "1",
    [OUT_DIM] = "2",
    [OUT_RESET] = "0",
    [OUT_ERROR] = "1;31",
    [OUT_WARNING] = "1;33",
    [OUT_HINT] = "36",
    [OUT_OK] = "32",
};

#define STYLE_COUNT ((int)(sizeof style_codes / sizeof style_codes[0]))

/* One buffer per style: two different styles never share storage, and a
 * style's own buffer always holds the same text. */
static char style_bufs[STYLE_COUNT][16];

const char *out_style(FILE *stream, out_style_t style)
{
    int index = (int)style;
    if (index < 0 || index >= STYLE_COUNT)
    {
        return "";
    }

    char *buf = style_bufs[index];
    if (!out_color_enabled(stream))
    {
        buf[0] = '\0';
        return buf;
    }

    snprintf(buf, sizeof style_bufs[0], "\033[%sm", style_codes[index]);
    return buf;
}

/* ========================================================================= */
/* MESSAGE HELPERS                                                           */
/* ========================================================================= */

/* Shared body: "vyt: " + optional colored "label: " + formatted message. */
static void vmsg(FILE *stream, const char *label, out_style_t style,
                 const char *fmt, va_list ap)
{
    scan_env();

    fputs("vyt: ", stream);
    if (label != NULL)
    {
        fputs(out_style(stream, style), stream);
        fputs(label, stream);
        fputs(out_style(stream, OUT_RESET), stream);
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
    fputs(out_style(stream, OUT_OK), stream);
    fputs("\u2713", stream); /* the check mark */
    fputs(out_style(stream, OUT_RESET), stream);
    fputc(' ', stream);
    vfprintf(stream, fmt, ap);
    fputc('\n', stream);

    va_end(ap);
}

void out_status(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmsg(stream, NULL, OUT_RESET, fmt, ap);
    va_end(ap);
}

void out_error(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmsg(stream, "error", OUT_ERROR, fmt, ap);
    va_end(ap);
}

void out_warn(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmsg(stream, "warning", OUT_WARNING, fmt, ap);
    va_end(ap);
}

void out_hint(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmsg(stream, "hint", OUT_HINT, fmt, ap);
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
            memmove(buf, s, len + 1);
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

    /* memmove, not memcpy: out_safe_line() truncates a buffer into itself. */
    memmove(buf, s, keep);
    memcpy(buf + keep, "\u2026", 4); /* "…" + NUL */
    return buf;
}

char *out_safe_line(char *buf, size_t bufsz, const char *s)
{
    if (bufsz == 0)
    {
        return buf;
    }

    size_t j = 0;
    for (size_t i = 0; s[i] != '\0' && j + 1 < bufsz; i++)
    {
        unsigned char c = (unsigned char)s[i];
        buf[j++] = (c < 0x20 || c == 0x7f) ? ' ' : (char)c;
    }
    buf[j] = '\0';

    return out_truncate_utf8(buf, bufsz, buf, bufsz - 1);
}

/* ========================================================================= */
/* LAYOUT HELPERS                                                            */
/* ========================================================================= */

size_t out_num_cols(size_t n)
{
    size_t cols = 1;
    while (n >= 10)
    {
        n /= 10;
        cols++;
    }
    return cols;
}

size_t out_fit_cols(int width, size_t indent, size_t hard_max)
{
    if (width <= 0)
    {
        return 0;
    }

    /* One column of margin keeps the text off the right edge. */
    size_t used = indent + 1;
    if ((size_t)width <= used)
    {
        return 0;
    }

    size_t available = (size_t)width - used;
    return (available < hard_max) ? available : hard_max;
}

void out_print_dim(FILE *stream, size_t indent, const char *text)
{
    fputs(out_style(stream, OUT_DIM), stream);
    fprintf(stream, "%*s", (int)indent, "");
    fputs(text, stream);
    fputs(out_style(stream, OUT_RESET), stream);
    fputc('\n', stream);
}
