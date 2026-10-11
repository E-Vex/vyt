#define _POSIX_C_SOURCE 200809L

/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "output.h"
#include "proc.h"
#include "search.h"

/* yt-dlp prints "NA" for fields it could not extract from a flat entry. */
#define YTNA "NA"

/* Unit separator between template fields.  Written as an octal escape:
 * a hex escape here would greedily absorb following hex-digit-looking
 * template text, and 037 cannot be confused with anything. */
#define SEP "\037"

#define SEARCH_PREFIX "ytsearch10:"

/* Flat search entries reliably carry title and url; duration and channel
 * are best-effort and simply read "NA" when missing, so no extra network
 * requests are made just for metadata. */
#define SEARCH_TEMPLATE "%(title)s" SEP "%(duration)s" SEP "%(channel)s" SEP "%(webpage_url)s"

/* ========================================================================= */
/* LINE PARSING                                                              */
/* ========================================================================= */

/* Copy [begin, end), replacing control characters (including any stray
 * separator bytes and terminal escapes) with spaces.  Titles come from
 * arbitrary YouTube metadata and must never inject escape sequences into
 * the terminal. */
static char *copy_sanitized(const char *begin, const char *end)
{
    size_t len = (size_t)(end - begin);
    char *copy = malloc(len + 1);
    if (copy == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)begin[i];
        copy[i] = (c < 0x20 || c == 0x7f) ? ' ' : (char)c;
    }
    copy[len] = '\0';
    return copy;
}

static char *sanitize_strdup(const char *s)
{
    return copy_sanitized(s, s + strlen(s));
}

int search_parse_line(const char *line, search_entry_t *e)
{
    memset(e, 0, sizeof *e);
    e->duration = -1.0;

    /* Scan for the three separators left to right.  The URL, channel and
     * duration fields cannot contain the separator, so the title is
     * everything before the first one. */
    const char *sep1 = strchr(line, SEP[0]);
    if (sep1 == NULL)
    {
        return -1;
    }
    const char *sep2 = strchr(sep1 + 1, SEP[0]);
    if (sep2 == NULL)
    {
        return -1;
    }
    const char *sep3 = strchr(sep2 + 1, SEP[0]);
    if (sep3 == NULL)
    {
        return -1;
    }

    e->title = copy_sanitized(line, sep1);
    char *duration = copy_sanitized(sep1 + 1, sep2);
    char *channel = copy_sanitized(sep2 + 1, sep3);
    char *url = sanitize_strdup(sep3 + 1);

    if (e->title == NULL || duration == NULL || channel == NULL ||
        url == NULL)
    {
        free(e->title);
        free(duration);
        free(channel);
        free(url);
        memset(e, 0, sizeof *e);
        return -1;
    }

    if (url[0] != '\0')
    {
        e->url = url;
    }
    else
    {
        free(url);
    }

    if (channel[0] != '\0' && strcmp(channel, YTNA) != 0)
    {
        e->channel = channel;
    }
    else
    {
        free(channel);
    }

    char *end = NULL;
    double d = strtod(duration, &end);
    if (end != duration && *end == '\0' && d > 0.0)
    {
        e->duration = d;
    }
    free(duration);

    return 0;
}

void search_entry_free(search_entry_t *e)
{
    if (e == NULL)
    {
        return;
    }
    free(e->title);
    free(e->url);
    free(e->channel);
    e->title = NULL;
    e->url = NULL;
    e->channel = NULL;
}

char *search_format_duration(char *buf, size_t bufsz, double sec)
{
    if (!(sec > 0.0))
    {
        return NULL; /* NA, 0, negative or NaN */
    }

    long total = (long)(sec + 0.5);
    if (total >= 3600)
    {
        snprintf(buf, bufsz, "%ld:%02ld:%02ld", total / 3600,
                 (total / 60) % 60, total % 60);
    }
    else
    {
        snprintf(buf, bufsz, "%ld:%02ld", total / 60, total % 60);
    }
    return buf;
}

/* ========================================================================= */
/* RESULT PRINTING                                                           */
/* ========================================================================= */

/* Maximum columns we are willing to spend on a title before truncating,
 * regardless of how wide the terminal reports itself.  Keeps the per-entry
 * stack buffers bounded on unusual terminals. */
#define TITLE_MAX_COLS 800

/* A one-line query safe to echo back: control characters become spaces
 * and the result is truncated, so a query can never rearrange the header. */
static void display_query(char *buf, size_t bufsz, const char *query)
{
    char clean[128];
    size_t j = 0;
    for (size_t i = 0; query[i] != '\0' && j < sizeof clean - 1; i++)
    {
        unsigned char c = (unsigned char)query[i];
        clean[j++] = (c < 0x20 || c == 0x7f) ? ' ' : (char)c;
    }
    clean[j] = '\0';

    out_truncate_utf8(buf, bufsz, clean, sizeof clean - 1);
}

void search_print_results(FILE *out, const char *query,
                          const search_entry_t *entries, size_t count,
                          int pretty)
{
    if (!pretty)
    {
        /* Historical scriptable format: one "title | url" line per result,
         * byte-identical to what `yt-dlp --print '%(title)s | %(webpage_url)s'`
         * used to emit through this command. */
        for (size_t i = 0; i < count; i++)
        {
            if (entries[i].url != NULL)
            {
                fprintf(out, "%s | %s\n", entries[i].title, entries[i].url);
            }
            else
            {
                fprintf(out, "%s\n", entries[i].title);
            }
        }
        return;
    }

    char q[160];
    display_query(q, sizeof q, query);

    fprintf(out, "%s%zu result%s for '%s':%s\n", out_sgr(out, "1"), count,
            (count == 1) ? "" : "s", q, out_reset(out));

    int width = out_term_width(out);

    /* Column count of the largest index, minimum 2 so " 1." aligns with
     * "10." and the metadata/URL lines under it. */
    size_t num_cols = 2;
    for (size_t n = count; n >= 10; n /= 10)
    {
        num_cols++;
    }
    size_t indent = num_cols + 2; /* "NN. " */

    for (size_t i = 0; i < count; i++)
    {
        const search_entry_t *e = &entries[i];

        const char *title =
            (e->title[0] != '\0' && strcmp(e->title, YTNA) != 0)
                ? e->title
                : "(untitled)";

        size_t max_title = TITLE_MAX_COLS;
        if (width > (int)(indent + 1) &&
            (size_t)width - indent - 1 < max_title)
        {
            max_title = (size_t)width - indent - 1;
        }
        char tbuf[TITLE_MAX_COLS + 8];
        out_truncate_utf8(tbuf, sizeof tbuf, title, max_title);

        fprintf(out, "%s%*zu.%s %s%s%s\n", out_sgr(out, "2"),
                (int)num_cols, i + 1, out_reset(out), out_sgr(out, "1"),
                tbuf, out_reset(out));

        char dbuf[32];
        const char *dur =
            search_format_duration(dbuf, sizeof dbuf, e->duration);
        int has_dur = (dur != NULL);
        int has_chan = (e->channel != NULL);

        if (has_dur || has_chan)
        {
            fputs(out_sgr(out, "2"), out);
            fprintf(out, "%*s", (int)indent, "");
            if (has_dur)
            {
                fputs(dur, out);
            }
            if (has_dur && has_chan)
            {
                fputs(" \u00b7 ", out); /* "·" */
            }
            if (has_chan)
            {
                size_t used = indent + (has_dur ? strlen(dur) : 0) + 3;
                size_t max_chan = 40;
                if (width > (int)(used + 1) &&
                    (size_t)width - used - 1 > max_chan)
                {
                    max_chan = (size_t)width - used - 1;
                }
                if (max_chan > 200)
                {
                    max_chan = 200;
                }
                char cbuf[820];
                out_truncate_utf8(cbuf, sizeof cbuf, e->channel, max_chan);
                fputs(cbuf, out);
            }
            fputs(out_reset(out), out);
            fputc('\n', out);
        }

        if (e->url != NULL)
        {
            /* Never truncate the URL: it is what the user copies into
             * `vyt -m` or `vyt -v`.  A very long URL wraps, which is fine
             * for a line whose only job is to be selected. */
            fputs(out_sgr(out, "2"), out);
            fprintf(out, "%*s%s", (int)indent, "", e->url);
            fputs(out_reset(out), out);
            fputc('\n', out);
        }
    }

    fprintf(out, "\n%splay one with: vyt -m <URL>%s\n", out_sgr(out, "2"),
            out_reset(out));
}

/* ========================================================================= */
/* COMMAND                                                                   */
/* ========================================================================= */

/* Growable vector of parsed results fed by the proc callback. */
typedef struct
{
    search_entry_t *items;
    size_t count;
    size_t capacity;
    size_t skipped; /* malformed lines seen */
} entry_vec_t;

static void on_result_line(const char *line, void *user_data)
{
    entry_vec_t *v = user_data;

    search_entry_t e;
    if (search_parse_line(line, &e) != 0)
    {
        v->skipped++;
        return;
    }

    if (v->count == v->capacity)
    {
        size_t new_capacity = (v->capacity == 0) ? 16 : v->capacity * 2;
        search_entry_t *grown =
            realloc(v->items, new_capacity * sizeof *grown);
        if (grown == NULL)
        {
            search_entry_free(&e); /* drop this result, keep the rest */
            return;
        }
        v->items = grown;
        v->capacity = new_capacity;
    }

    v->items[v->count++] = e;
}

static void entries_free(entry_vec_t *v)
{
    for (size_t i = 0; i < v->count; i++)
    {
        search_entry_free(&v->items[i]);
    }
    free(v->items);
    v->items = NULL;
    v->count = 0;
    v->capacity = 0;
}

int search_youtube(const char *query)
{
    size_t len = strlen(SEARCH_PREFIX) + strlen(query) + 1;
    char *search_term = malloc(len);
    if (search_term == NULL)
    {
        out_error(stderr, "out of memory");
        return -1;
    }
    snprintf(search_term, len, "%s%s", SEARCH_PREFIX, query);

    char *argv[] = {
        "yt-dlp",
        "--flat-playlist",
        "--print",
        SEARCH_TEMPLATE,
        "--",
        search_term,
        NULL
    };

    entry_vec_t results = {0};
    int status = proc_run_capture("yt-dlp", argv, on_result_line, &results);
    free(search_term);

    if (status < 0)
    {
        /* vyt itself failed to spawn yt-dlp; proc already reported why. */
        entries_free(&results);
        return -1;
    }

    if (results.count > 0)
    {
        search_print_results(stdout, query, results.items, results.count,
                             out_stdout_is_tty());
    }

    if (results.skipped > 0)
    {
        out_warn(stderr, "skipped %zu malformed result line%s from yt-dlp",
                 results.skipped, (results.skipped == 1) ? "" : "s");
    }

    if (status != 0)
    {
        if (status != 127)
        {
            /* 127 means execvp already explained itself ("not found");
             * adding another error line would repeat the same news. */
            out_error(stderr, "yt-dlp exited with status %d", status);
            out_hint(stderr,
                     "if this keeps happening, update yt-dlp "
                     "(YouTube changes break older versions)");
        }
    }
    else if (results.count == 0)
    {
        char q[160];
        display_query(q, sizeof q, query);
        out_status(stderr, "no results for '%s'", q);
        out_hint(stderr, "try different keywords");
    }

    entries_free(&results);
    return status;
}
