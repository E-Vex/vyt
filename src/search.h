#ifndef SEARCH_H
#define SEARCH_H

#include <stddef.h>
#include <stdio.h>

/* yt-dlp prints "NA" for fields it could not extract from a flat entry. */
#define SEARCH_NA "NA"

/*
 * One parsed result line from yt-dlp.
 *
 * Fields are heap-owned; free them with search_entry_free().
 *   title    never NULL, but may be "" (or the literal "NA" yt-dlp prints
 *            for missing fields) -- display code maps both to a placeholder
 *   url      NULL when the line carried no URL
 *   duration seconds; negative when unknown
 *   channel  NULL when unknown
 */
typedef struct
{
    char *title;
    char *url;
    double duration;
    char *channel;
} search_entry_t;

/*
 * Search YouTube for `query` via yt-dlp (top 10 results) and print the
 * results.  On an interactive terminal the output is a numbered, formatted
 * list; when stdout is redirected the output is the historical
 * "title | URL" one-line-per-result format so shell scripts keep working.
 *
 * Returns yt-dlp's exit status (0 on success, including "no results"),
 * or -1 if vyt itself failed to spawn yt-dlp.
 */
int search_youtube(const char *query);

/*
 * Split one yt-dlp output line into a search_entry_t.
 *
 * The template vyt runs yt-dlp with is
 *   %(title)s <US> %(duration)s <US> %(channel)s <US> %(webpage_url)s
 * where <US> is the ASCII unit separator (\x1f), so titles containing
 * " | " or other punctuation never confuse the parser.
 *
 * Returns 0 on success, -1 if the line is not a well-formed result line
 * (the caller counts it as malformed and reports it).
 */
int search_parse_line(const char *line, search_entry_t *e);

/* Free the heap fields of `e` (the struct itself is the caller's). */
void search_entry_free(search_entry_t *e);

/*
 * Render `sec` as M:SS or H:MM:SS into buf.  Returns buf, or NULL when the
 * duration is unknown (<= 0), which callers display as "no duration".
 */
char *search_format_duration(char *buf, size_t bufsz, double sec);

/*
 * Print `count` results to `out`.
 *
 * pretty != 0 selects the human format: a bold header naming the query and
 * result count, then each result as a numbered entry with an optional
 * duration/channel line and a dimmed URL line, then a one-line hint for
 * playing a result.  pretty == 0 selects the scriptable format: exactly
 * one "title | URL" line per result, no header, no footer.
 *
 * The URL is never truncated (it must remain copy-pasteable); titles and
 * channel names are truncated to the terminal width at a UTF-8 boundary.
 */
void search_print_results(FILE *out, const char *query,
                          const search_entry_t *entries, size_t count,
                          int pretty);

#endif /* SEARCH_H */
