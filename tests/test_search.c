#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/search.h"

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

#define SEP "\x1f"

/* ------------------------------------------------------------------------ */
/* parsing                                                                   */
/* ------------------------------------------------------------------------ */

static void test_parse_full_line(void)
{
    search_entry_t e;
    char line[256];
    snprintf(line, sizeof line, "Kavinsky - Nightcall" SEP "256" SEP
             "Kavinsky" SEP "https://www.youtube.com/watch?v=S0");

    CHECK(search_parse_line(line, &e) == 0, "parse: full line succeeds");
    CHECK_STREQ(e.title, "Kavinsky - Nightcall", "parse: title");
    CHECK(e.url != NULL && strcmp(e.url, "https://www.youtube.com/watch?v=S0") == 0,
          "parse: url");
    CHECK_STREQ(e.channel, "Kavinsky", "parse: channel");
    CHECK(e.duration > 255.9 && e.duration < 256.1, "parse: duration");
    search_entry_free(&e);
}

static void test_parse_na_fields(void)
{
    search_entry_t e;
    char line[256];
    snprintf(line, sizeof line, "Some Title" SEP "NA" SEP "NA" SEP
             "https://youtu.be/xyz");

    CHECK(search_parse_line(line, &e) == 0, "parse: NA line succeeds");
    CHECK_STREQ(e.title, "Some Title", "parse: NA title kept raw");
    CHECK(e.duration < 0, "parse: NA duration -> unknown");
    CHECK(e.channel == NULL, "parse: NA channel -> NULL");
    CHECK(e.url != NULL, "parse: url present");
    search_entry_free(&e);
}

static void test_parse_title_with_pipes(void)
{
    search_entry_t e;
    char line[256];
    snprintf(line, sizeof line, "Song | Official Video | HD" SEP "61" SEP
             "Artist" SEP "https://youtu.be/pipes");

    CHECK(search_parse_line(line, &e) == 0, "parse: pipe-heavy title succeeds");
    CHECK_STREQ(e.title, "Song | Official Video | HD",
                "parse: separators never touch the title");
    search_entry_free(&e);
}

static void test_parse_malformed(void)
{
    search_entry_t e;

    CHECK(search_parse_line("no separators at all", &e) == -1,
          "parse: no separator rejected");
    CHECK(search_parse_line("title" SEP "url", &e) == -1,
          "parse: two fields rejected");
    CHECK(search_parse_line("t" SEP "d" SEP "c", &e) == -1,
          "parse: three fields rejected");
    CHECK(search_parse_line("", &e) == -1, "parse: empty line rejected");
}

static void test_parse_empty_url(void)
{
    search_entry_t e;
    char line[256];
    snprintf(line, sizeof line, "Title" SEP "30" SEP "Chan" SEP "");

    CHECK(search_parse_line(line, &e) == 0, "parse: empty url accepted");
    CHECK(e.url == NULL, "parse: empty url -> NULL");
    search_entry_free(&e);
}

static void test_parse_strips_control_characters(void)
{
    search_entry_t e;
    char line[256];
    snprintf(line, sizeof line, "Bad\033[31mTitle" SEP "30" SEP "Ch\007an" SEP
             "https://youtu.be/ctrl");

    CHECK(search_parse_line(line, &e) == 0, "parse: control-char line accepted");
    CHECK_STREQ(e.title, "Bad [31mTitle",
                "parse: ESC replaced (bracket survives, escape cannot)");
    CHECK_STREQ(e.channel, "Ch an", "parse: bell replaced with space");
    search_entry_free(&e);
}

/* ------------------------------------------------------------------------ */
/* duration formatting                                                       */
/* ------------------------------------------------------------------------ */

static void test_format_duration(void)
{
    char buf[32];

    CHECK(search_format_duration(buf, sizeof buf, 45) != NULL &&
              strcmp(buf, "0:45") == 0, "duration: 45s -> 0:45");
    CHECK(search_format_duration(buf, sizeof buf, 245) != NULL &&
              strcmp(buf, "4:05") == 0, "duration: 245s -> 4:05");
    CHECK(search_format_duration(buf, sizeof buf, 3599) != NULL &&
              strcmp(buf, "59:59") == 0, "duration: 3599s -> 59:59");
    CHECK(search_format_duration(buf, sizeof buf, 3600) != NULL &&
              strcmp(buf, "1:00:00") == 0, "duration: 3600s -> 1:00:00");
    CHECK(search_format_duration(buf, sizeof buf, 3672) != NULL &&
              strcmp(buf, "1:01:12") == 0, "duration: 3672s -> 1:01:12");
    CHECK(search_format_duration(buf, sizeof buf, 0) == NULL,
          "duration: 0 -> NULL");
    CHECK(search_format_duration(buf, sizeof buf, -5) == NULL,
          "duration: negative -> NULL");
}

/* ------------------------------------------------------------------------ */
/* printing                                                                  */
/* ------------------------------------------------------------------------ */

static char *print_results(const search_entry_t *entries, size_t count,
                           int pretty)
{
    char *buf = NULL;
    size_t len = 0;
    FILE *ms = open_memstream(&buf, &len);
    if (ms == NULL)
    {
        return NULL;
    }
    search_print_results(ms, "test query", entries, count, pretty);
    fclose(ms);
    return buf;
}

static search_entry_t make_entry(const char *title, double duration,
                                 const char *channel, const char *url)
{
    search_entry_t e;
    e.title = strdup(title);
    e.url = url ? strdup(url) : NULL;
    e.duration = duration;
    e.channel = channel ? strdup(channel) : NULL;
    return e;
}

static void test_print_scriptable_format(void)
{
    search_entry_t entries[2] = {
        make_entry("Title One", -1, NULL, "https://youtu.be/one"),
        make_entry("Title Two", -1, NULL, "https://youtu.be/two"),
    };

    char *s = print_results(entries, 2, 0);
    CHECK(s != NULL, "print: scriptable format produced output");
    if (s != NULL)
    {
        CHECK_STREQ(s, "Title One | https://youtu.be/one\n"
                       "Title Two | https://youtu.be/two\n",
                    "print: scriptable format is legacy title | url");
        CHECK(strstr(s, "result") == NULL,
              "print: no header in scriptable format");
        CHECK(strchr(s, '\033') == NULL, "print: no escapes in scriptable");
    }
    free(s);

    entries[0].duration = 245;
    entries[0].channel = strdup("Chan");
    s = print_results(entries, 2, 0);
    CHECK(s != NULL && strstr(s, "4:05") == NULL && strstr(s, "Chan") == NULL,
          "print: metadata omitted in scriptable format");
    free(s);

    search_entry_free(&entries[0]);
    search_entry_free(&entries[1]);
}

static void test_print_pretty_format(void)
{
    search_entry_t entries[2] = {
        make_entry("Kavinsky - Nightcall", 256, "Kavinsky",
                   "https://www.youtube.com/watch?v=abc"),
        make_entry("Second Song", -1, NULL, "https://youtu.be/def"),
    };

    char *s = print_results(entries, 2, 1);
    CHECK(s != NULL, "print: pretty format produced output");
    if (s != NULL)
    {
        CHECK(strstr(s, "2 results for 'test query':") == s,
              "print: header names the query and count");
        CHECK(strstr(s, " 1. Kavinsky - Nightcall\n") != NULL,
              "print: first entry numbered");
        CHECK(strstr(s, " 2. Second Song\n") != NULL,
              "print: second entry numbered");
        CHECK(strstr(s, "    4:16 \xc2\xb7 Kavinsky\n") != NULL,
              "print: metadata line with duration and channel");
        CHECK(strstr(s, "    https://www.youtube.com/watch?v=abc\n") != NULL,
              "print: url line indented");
        CHECK(strstr(s, "play one with: vyt -m <URL>") != NULL,
              "print: footer hint present");
        CHECK(strstr(s, "4:16") == strstr(s, "4:16") &&
              strstr(s, "\033") == NULL,
              "print: no escapes with color off");
    }
    free(s);

    /* Singular header. */
    char *s1 = print_results(entries, 1, 1);
    CHECK(s1 != NULL && strstr(s1, "1 result for 'test query':") == s1,
          "print: singular result header");
    free(s1);

    search_entry_free(&entries[0]);
    search_entry_free(&entries[1]);
}

static void test_print_pretty_untitled_and_missing_url(void)
{
    search_entry_t entries[2] = {
        make_entry("NA", -1, NULL, "https://youtu.be/nA"),
        make_entry("", -1, NULL, NULL),
    };

    char *s = print_results(entries, 2, 1);
    CHECK(s != NULL, "print: degenerate entries produce output");
    if (s != NULL)
    {
        CHECK(strstr(s, " 1. (untitled)\n") != NULL,
              "print: NA title becomes (untitled)");
        CHECK(strstr(s, " 2. (untitled)\n") != NULL,
              "print: empty title becomes (untitled)");
        CHECK(strstr(s, "https://youtu.be/nA") != NULL,
              "print: url still shown for NA title");
    }
    free(s);

    search_entry_free(&entries[0]);
    search_entry_free(&entries[1]);
}

static void test_print_pretty_truncates_long_title(void)
{
    char long_title[200];
    memset(long_title, 'x', sizeof long_title - 1);
    long_title[sizeof long_title - 1] = '\0';

    search_entry_t entries[1] = {
        make_entry(long_title, -1, NULL, "https://youtu.be/long"),
    };

    char *s = print_results(entries, 1, 1);
    CHECK(s != NULL, "print: long title produces output");
    if (s != NULL)
    {
        CHECK(strstr(s, "\u2026") != NULL, "print: long title truncated");
        CHECK(strstr(s, "https://youtu.be/long") != NULL,
              "print: url never truncated");
        CHECK(strlen(s) < 400, "print: truncated output stays bounded");
    }
    free(s);

    search_entry_free(&entries[0]);
}

static void test_print_empty_query_sanitized(void)
{
    search_entry_t entries[1] = {
        make_entry("T", -1, NULL, "https://youtu.be/q"),
    };

    char *buf = NULL;
    size_t len = 0;
    FILE *ms = open_memstream(&buf, &len);
    CHECK(ms != NULL, "print: memstream opened");
    if (ms != NULL)
    {
        search_print_results(ms, "query\nwith\nnewlines\x1f", entries, 1, 1);
        fclose(ms);
        CHECK(strchr(buf, '\n') == strrchr(buf, '\n') - 1 ||
                  strstr(buf, "query with newlines") != NULL,
              "print: control chars in query neutralized");
        CHECK(strstr(buf, "query with newlines") != NULL,
              "print: query echoed as one safe line");
        free(buf);
    }

    search_entry_free(&entries[0]);
}

int main(void)
{
    unsetenv("COLUMNS"); /* keep terminal-width math deterministic */
    unsetenv("NO_COLOR");
    unsetenv("CLICOLOR");
    unsetenv("CLICOLOR_FORCE");
    unsetenv("TERM");

    test_parse_full_line();
    test_parse_na_fields();
    test_parse_title_with_pipes();
    test_parse_malformed();
    test_parse_empty_url();
    test_parse_strips_control_characters();
    test_format_duration();
    test_print_scriptable_format();
    test_print_pretty_format();
    test_print_pretty_untitled_and_missing_url();
    test_print_pretty_truncates_long_title();
    test_print_empty_query_sanitized();

    if (failures == 0)
    {
        printf("All %d checks passed.\n", checks);
        return 0;
    }

    fprintf(stderr, "%d/%d checks failed.\n", failures, checks);
    return 1;
}
