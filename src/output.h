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

/*
 * The named styles vyt uses.
 *
 * A style is one piece of formatting: bold, dim, a colored label, ...  The
 * escape sequence behind a style is fixed, so the same style may appear
 * several times in one printf() without the results interfering with each
 * other -- there is no buffer ring to exhaust and no attribute to garble.
 */
typedef enum
{
    OUT_BOLD,    /* headings, result titles              */
    OUT_DIM,     /* secondary text: URLs, metadata        */
    OUT_RESET,   /* back to the terminal's default        */
    OUT_ERROR,   /* the "error" label                     */
    OUT_WARNING, /* the "warning" label                   */
    OUT_HINT,    /* the "hint" label                      */
    OUT_OK       /* the success check mark                */
} out_style_t;

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
 * The escape sequence for `style` on `stream`, or "" when color is off.
 *
 * The returned string stays valid until the next out_style() call for the
 * same style, and its contents never change for a given style, so several
 * styles can be composed inside a single printf() safely.  The color
 * decision follows the stream passed in, so keep one stream per printf().
 */
const char *out_style(FILE *stream, out_style_t style);

/*
 * Message helpers.  All of them print one line, prefixed with "vyt: ".
 * out_ok adds a green check mark; out_error/out_warn/out_hint add a
 * colored label.  Formatting of `fmt`/... is printf-style; callers may
 * embed out_style() results for finer-grained emphasis.
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

/*
 * Copy `s` into buf as one safe line: control characters (including
 * terminal escapes) become spaces, and the result is truncated to fit.
 *
 * Use this for text that comes from the user and is echoed back in a
 * message, such as a search query: it must never be able to rearrange the
 * line it is printed on.
 */
char *out_safe_line(char *buf, size_t bufsz, const char *s);

/* ========================================================================= */
/* LAYOUT HELPERS                                                             */
/*                                                                            */
/* The listing commands (search results, playlist songs) share one shape:    */
/* a numbered entry, optional metadata, and a dimmed URL underneath.  The   */
/* helpers below hold the arithmetic that shape needs, so the two listings  */
/* cannot drift apart.                                                        */
/* ========================================================================= */

/* Columns the index of a numbered list occupies: 1 for "1.", 2 for "10.",
 * 3 for "100.", and so on. */
size_t out_num_cols(size_t n);

/* How many columns of text fit next to `indent` columns of indentation on
 * a terminal `width` columns wide, never more than `hard_max`.
 * One column is always left empty as a margin, so text never touches the
 * right edge.  Returns 0 when the terminal is too narrow for any text. */
size_t out_fit_cols(int width, size_t indent, size_t hard_max);

/* Print `text` on a line of its own, dimmed and indented by `indent`
 * columns. */
void out_print_dim(FILE *stream, size_t indent, const char *text);

#endif /* OUTPUT_H */
