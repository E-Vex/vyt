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

    if (channel[0] != '\0' && strcmp(channel, SEARCH_NA) != 0)
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
        out_safe_line(q, sizeof q, query);
        out_status(stderr, "no results for '%s'", q);
        out_hint(stderr, "try different keywords");
    }

    entries_free(&results);
    return status;
}
