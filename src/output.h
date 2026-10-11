#ifndef OUTPUT_H
#define OUTPUT_H

#include <stddef.h>
#include <stdio.h>

/*
 * vyt's shared terminal-output layer.
 *
 * Every user-facing message goes through this module so the whole program
 * speaks one visual language:
 *
 *   vyt: created playlist 'chill'        success  (green check mark)
 *   vyt: no playlists yet                neutral status
 *   vyt: error: no such playlist         error    (red label)
 *   vyt: warning: skipping entry         warning  (yellow label)
 *   vyt: hint: run 'vyt playlist list'   next step (cyan label)
 *
 * Color is a rendering detail, never content: the same words are emitted
 * with and without ANSI escapes.  Escapes are written only when the target
 * stream is an interactive terminal, and only if the environment allows it.
 *
 * Environment conventions, in order of precedence:
 *   1. out_set_color_mode()   programmatic override (tests, embedders)
 *   2. NO_COLOR               non-empty value disables color everywhere
 *   3. CLICOLOR_FORCE         non-empty value other than "0" forces color on
 *   4. CLICOLOR=0             disables color
 *   5. TERM=dumb              disables color
 *   6. otherwise: color iff the target stream is a terminal
 *
 * Formatting decisions that are NOT about color (numbered lists, tables,
 * column alignment) use out_stdout_is_tty(): when stdout is redirected the
 * program falls back to stable, grep-friendly plain formats so shell
 * scripts keep working.  See the "Scriptable output" notes in README.md.
 */

/* Color mode for out_set_color_mode(). */
typedef enum
{
    OUT_COLOR_AUTO = -1, /* decide from isatty() and the environment */
    OUT_COLOR_OFF = 0,   /* never emit escapes */
    OUT_COLOR_ON = 1     /* always emit escapes */
} out_color_mode_t;

/* Scan the environment once.  Idempotent; the helpers call it lazily, so
 * only programs that care about env changes before the first message need
 * to call it explicitly (main.c does, for clarity). */
void out_init(void);

/* Programmatic override; OUT_COLOR_AUTO returns control to the environment.
 * Highest precedence, intended for tests and embedders. */
void out_set_color_mode(out_color_mode_t mode);

/* May ANSI escapes be written to this stream right now? */
int out_color_enabled(FILE *stream);

/* Is stdout an interactive terminal?  Cached at first call.
 * Drives rich-vs-plain format selection (independent of color). */
int out_stdout_is_tty(void);

/* Terminal width of stream: ioctl(TIOCGWINSZ), then $COLUMNS, then 80. */
int out_term_width(FILE *stream);

/*
 * Return the SGR escape for `code` (e.g. "1" bold, "2" dim, "31" red,
 * "1;31" bold red), or "" when color is off for `stream`.
 *
 * Successive calls draw from a small ring of static buffers, so several
 * out_sgr()/out_reset() results may be composed inside a single printf().
 * Up to 16 distinct results per call site are safe; more is a programming
 * error and produces garbled attributes.  Code must be at most 19 chars.
 */
const char *out_sgr(FILE *stream, const char *code);
const char *out_reset(FILE *stream);

/*
 * Message helpers.  All of them print one line, prefixed with "vyt: ".
 * out_ok adds a green check mark; out_error/out_warn/out_hint add a
 * colored label.  Formatting of `fmt`/`...` is printf-style; callers may
 * embed out_sgr()/out_reset() results for finer-grained emphasis.
 */
void out_ok(FILE *stream, const char *fmt, ...);
void out_status(FILE *stream, const char *fmt, ...);
void out_error(FILE *stream, const char *fmt, ...);
void out_warn(FILE *stream, const char *fmt, ...);
void out_hint(FILE *stream, const char *fmt, ...);

/*
 * Copy `s` into buf, never exceeding max_cols bytes and never splitting a
 * UTF-8 sequence; when truncation happens, the last column becomes "…".
 * Byte length is used as a column-width approximation, so wide characters
 * (CJK) truncate slightly early -- the safe direction.  buf must be large
 * enough (max_cols + 4 bytes covers the worst case).
 */
char *out_truncate_utf8(char *buf, size_t bufsz, const char *s,
                        size_t max_cols);

#endif /* OUTPUT_H */
