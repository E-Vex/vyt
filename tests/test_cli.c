#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/cli.h"

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

/* Parse `argv` and return the result; the caller frees opts.argument. */
static int run_parse(char *argv[], options_t *opts)
{
    int argc = 0;
    while (argv[argc] != NULL)
    {
        argc++;
    }
    optind = 0; /* portable full reset of getopt state between test cases */
    return cli_parse(argc, argv, opts);
}

/* The parse succeeds and picks the expected mode. */
static void expect_mode(char *argv[], cli_mode_t want, const char *msg)
{
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == 0, msg);
    CHECK(opts.mode == want, msg);
    free(opts.argument);
}

/* The parse succeeds and turns the argument into exactly `want`. */
static void expect_argument(char *argv[], const char *want, const char *msg)
{
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == 0, msg);
    if (rc == 0)
    {
        CHECK(opts.argument != NULL && strcmp(opts.argument, want) == 0, msg);
    }
    free(opts.argument);
}

/* The parse fails. */
static void expect_failure(char *argv[], const char *msg)
{
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == -1, msg);
    free(opts.argument);
}

/* ------------------------------------------------------------------------ */
/* short options                                                             */
/* ------------------------------------------------------------------------ */

static void test_music_mode(void)
{
    expect_mode((char *[]){"vyt", "-m", "https://youtu.be/abc", NULL},
                MODE_MUSIC, "music: expected MODE_MUSIC");
    expect_argument((char *[]){"vyt", "-m", "https://youtu.be/abc", NULL},
                    "https://youtu.be/abc", "music: expected argument to be the URL");
}

static void test_video_mode(void)
{
    expect_mode((char *[]){"vyt", "-v", "https://youtu.be/xyz", NULL},
                MODE_VIDEO, "video: expected MODE_VIDEO");
}

static void test_search_mode(void)
{
    expect_mode((char *[]){"vyt", "-s", "some query", NULL}, MODE_SEARCH,
                "search: expected MODE_SEARCH");
    expect_argument((char *[]){"vyt", "-s", "some query", NULL}, "some query",
                    "search: expected argument to be the query");
}

static void test_download_mode(void)
{
    expect_mode((char *[]){"vyt", "-d", "https://youtu.be/abc", NULL},
                MODE_DOWNLOAD, "download: expected MODE_DOWNLOAD");
}

static void test_help_mode(void)
{
    expect_mode((char *[]){"vyt", "-h", NULL}, MODE_HELP,
                "help: expected MODE_HELP");
}

static void test_version_mode(void)
{
    expect_mode((char *[]){"vyt", "-V", NULL}, MODE_VERSION,
                "version: expected MODE_VERSION");
}

/* ------------------------------------------------------------------------ */
/* long options                                                              */
/* ------------------------------------------------------------------------ */

static void test_long_options(void)
{
    expect_mode((char *[]){"vyt", "--music", "https://youtu.be/abc", NULL},
                MODE_MUSIC, "long music: expected MODE_MUSIC");
    expect_mode((char *[]){"vyt", "--video", "https://youtu.be/abc", NULL},
                MODE_VIDEO, "long video: expected MODE_VIDEO");
    expect_mode((char *[]){"vyt", "--search", "some query", NULL}, MODE_SEARCH,
                "long search: expected MODE_SEARCH");
    expect_mode((char *[]){"vyt", "--download", "https://youtu.be/abc", NULL},
                MODE_DOWNLOAD, "long download: expected MODE_DOWNLOAD");
    expect_mode((char *[]){"vyt", "--help", NULL}, MODE_HELP,
                "long help: expected MODE_HELP");
    expect_mode((char *[]){"vyt", "--version", NULL}, MODE_VERSION,
                "long version: expected MODE_VERSION");
}

static void test_long_options_take_arguments(void)
{
    expect_argument((char *[]){"vyt", "--music", "https://youtu.be/abc", NULL},
                    "https://youtu.be/abc", "long music: expected argument");
    expect_argument((char *[]){"vyt", "--search", "some query", NULL},
                    "some query", "long search: expected argument");
}

static void test_long_option_errors(void)
{
    expect_failure((char *[]){"vyt", "--bogus", NULL},
                   "unknown long option: expected failure");
    expect_failure((char *[]){"vyt", "--music", NULL},
                   "long option without argument: expected failure");
    expect_failure((char *[]){"vyt", "--help", "--version", NULL},
                   "long help+version: expected failure");
}

/* ------------------------------------------------------------------------ */
/* media references                                                          */
/* ------------------------------------------------------------------------ */

static void test_video_id_shorthand(void)
{
    expect_argument((char *[]){"vyt", "-m", "dQw4w9WgXcQ", NULL},
                    "https://youtu.be/dQw4w9WgXcQ",
                    "shorthand: a video ID becomes a youtu.be link");
    expect_argument((char *[]){"vyt", "-d", "dQw4w9WgXcQ", NULL},
                    "https://youtu.be/dQw4w9WgXcQ",
                    "shorthand: works for downloads too");
}

static void test_scheme_less_url(void)
{
    expect_argument((char *[]){"vyt", "-v", "youtu.be/dQw4w9WgXcQ", NULL},
                    "https://youtu.be/dQw4w9WgXcQ",
                    "scheme-less: youtu.be gets a scheme");
    expect_argument((char *[]){"vyt", "-v", "www.youtube.com/watch?v=abc", NULL},
                    "https://www.youtube.com/watch?v=abc",
                    "scheme-less: youtube.com gets a scheme");
}

static void test_full_url_is_kept(void)
{
    expect_argument((char *[]){"vyt", "-m", "http://example.com/video", NULL},
                    "http://example.com/video",
                    "full URL: kept as it is");
}

static void test_surrounding_whitespace_is_ignored(void)
{
    expect_argument((char *[]){"vyt", "-m", "  dQw4w9WgXcQ\n", NULL},
                    "https://youtu.be/dQw4w9WgXcQ",
                    "whitespace: a pasted reference still works");
}

static void test_invalid_reference_is_rejected(void)
{
    expect_failure((char *[]){"vyt", "-m", "not a url", NULL},
                   "invalid reference: expected failure");
    expect_failure((char *[]){"vyt", "-m", "tooshort", NULL},
                   "invalid reference: expected failure");
}

static void test_search_query_is_left_alone(void)
{
    /* A query is free text: it must not be turned into a URL. */
    expect_argument((char *[]){"vyt", "-s", "dQw4w9WgXcQ", NULL},
                    "dQw4w9WgXcQ", "search: the query is not normalized");
    expect_argument((char *[]){"vyt", "-s", "not a url", NULL}, "not a url",
                    "search: a query may look like anything");
}

/* ------------------------------------------------------------------------ */
/* errors                                                                    */
/* ------------------------------------------------------------------------ */

static void test_no_mode_is_error(void)
{
    expect_failure((char *[]){"vyt", NULL}, "no mode: expected failure");
}

static void test_conflicting_modes_is_error(void)
{
    expect_failure((char *[]){"vyt", "-m", "a", "-v", "b", NULL},
                   "conflicting modes: expected failure");
}

static void test_help_with_other_flag_is_error(void)
{
    expect_failure((char *[]){"vyt", "-h", "-V", NULL},
                   "help+version: expected failure");
}

static void test_missing_argument_is_error(void)
{
    expect_failure((char *[]){"vyt", "-m", NULL},
                   "missing argument: expected failure");
}

static void test_unknown_option_is_error(void)
{
    expect_failure((char *[]){"vyt", "-x", NULL},
                   "unknown option: expected failure");
}

static void test_trailing_positional_is_error(void)
{
    expect_failure((char *[]){"vyt", "-h", "extra", NULL},
                   "trailing positional: expected failure");
}

static void test_empty_arguments_are_errors(void)
{
    expect_failure((char *[]){"vyt", "-m", "", NULL},
                   "empty -m argument: expected failure");
    expect_failure((char *[]){"vyt", "-v", "", NULL},
                   "empty -v argument: expected failure");
    expect_failure((char *[]){"vyt", "-s", "", NULL},
                   "empty -s argument: expected failure");
    expect_failure((char *[]){"vyt", "-d", "", NULL},
                   "empty -d argument: expected failure");
    expect_failure((char *[]){"vyt", "--music", "", NULL},
                   "empty --music argument: expected failure");
}

int main(void)
{
    test_music_mode();
    test_video_mode();
    test_search_mode();
    test_download_mode();
    test_help_mode();
    test_version_mode();

    test_long_options();
    test_long_options_take_arguments();
    test_long_option_errors();

    test_video_id_shorthand();
    test_scheme_less_url();
    test_full_url_is_kept();
    test_surrounding_whitespace_is_ignored();
    test_invalid_reference_is_rejected();
    test_search_query_is_left_alone();

    test_no_mode_is_error();
    test_conflicting_modes_is_error();
    test_help_with_other_flag_is_error();
    test_missing_argument_is_error();
    test_unknown_option_is_error();
    test_trailing_positional_is_error();
    test_empty_arguments_are_errors();

    if (failures == 0)
    {
        printf("All %d checks passed.\n", checks);
        return 0;
    }

    fprintf(stderr, "%d/%d checks failed.\n", failures, checks);
    return 1;
}
