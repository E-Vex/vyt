#define _POSIX_C_SOURCE 200809L

/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <stdio.h>
#include <string.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "output.h"
#include "search.h"

/* ========================================================================= */
/* FORMATTING                                                                */
/* ========================================================================= */

/* Maximum columns we are willing to spend on a title before truncating,
 * regardless of how wide the terminal reports itself.  Keeps the per-entry
 * stack buffers bounded on unusual terminals. */
#define TITLE_MAX_COLS 800

/* Maximum columns spent on a channel name. */
#define CHANNEL_MAX_COLS 40

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
    out_safe_line(q, sizeof q, query);

    fprintf(out, "%s%zu result%s for '%s':%s\n", out_style(out, OUT_BOLD),
            count, (count == 1) ? "" : "s", q, out_style(out, OUT_RESET));

    int width = out_term_width(out);

    /* Column count of the largest index, minimum 2 so " 1." aligns with
     * "10." and the metadata/URL lines under it. */
    size_t num_cols = out_num_cols(count);
    if (num_cols < 2)
    {
        num_cols = 2;
    }
    size_t indent = num_cols + 2; /* "NN. " */

    for (size_t i = 0; i < count; i++)
    {
        const search_entry_t *e = &entries[i];

        const char *title =
            (e->title[0] != '\0' && strcmp(e->title, SEARCH_NA) != 0)
                ? e->title
                : "(untitled)";

        size_t max_title = out_fit_cols(width, indent, TITLE_MAX_COLS);
        char tbuf[TITLE_MAX_COLS + 8];
        out_truncate_utf8(tbuf, sizeof tbuf, title, max_title);

        fprintf(out, "%s%*zu.%s %s%s%s\n", out_style(out, OUT_DIM),
                (int)num_cols, i + 1, out_style(out, OUT_RESET),
                out_style(out, OUT_BOLD), tbuf, out_style(out, OUT_RESET));

        char dbuf[32];
        const char *dur =
            search_format_duration(dbuf, sizeof dbuf, e->duration);
        int has_dur = (dur != NULL);
        int has_chan = (e->channel != NULL);

        if (has_dur || has_chan)
        {
            /* The metadata line: "4:13 · Kavinsky", dimmed like the URL
             * below it.  The channel is truncated to whatever room is
             * left on the terminal. */
            char mbuf[128];
            size_t used = 0;

            if (has_dur)
            {
                used += (size_t)snprintf(mbuf + used, sizeof mbuf - used, "%s",
                                         dur);
            }
            if (has_dur && has_chan)
            {
                used += (size_t)snprintf(mbuf + used, sizeof mbuf - used,
                                         " \u00b7 "); /* "·" */
            }
            if (has_chan)
            {
                size_t max_chan =
                    out_fit_cols(width, indent + used, CHANNEL_MAX_COLS);
                char cbuf[CHANNEL_MAX_COLS + 8];
                out_truncate_utf8(cbuf, sizeof cbuf, e->channel, max_chan);
                snprintf(mbuf + used, sizeof mbuf - used, "%s", cbuf);
            }

            out_print_dim(out, indent, mbuf);
        }

        if (e->url != NULL)
        {
            /* Never truncate the URL: it is what the user copies into
             * `vyt -m` or `vyt -v`.  A very long URL wraps, which is fine
             * for a line whose only job is to be selected. */
            out_print_dim(out, indent, e->url);
        }
    }

    fprintf(out, "\n%splay one with: vyt -m <URL>%s\n",
            out_style(out, OUT_DIM), out_style(out, OUT_RESET));
}
