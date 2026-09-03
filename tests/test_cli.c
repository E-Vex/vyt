#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
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

static int run_parse(char *argv[], options_t *opts)
{
    int argc = 0;
    while (argv[argc] != NULL)
    {
        argc++;
    }
    optind = 1; /* reset getopt state between test cases */
    return cli_parse(argc, argv, opts);
}

static void test_music_mode(void)
{
    char *argv[] = {"vyt", "-m", "https://youtu.be/abc", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == 0, "music: expected success");
    CHECK(opts.mode == MODE_MUSIC, "music: expected MODE_MUSIC");
    CHECK(opts.argument != NULL && strcmp(opts.argument, "https://youtu.be/abc") == 0,
          "music: expected argument to be the URL");
}

static void test_video_mode(void)
{
    char *argv[] = {"vyt", "-v", "https://youtu.be/xyz", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == 0, "video: expected success");
    CHECK(opts.mode == MODE_VIDEO, "video: expected MODE_VIDEO");
}

static void test_search_mode(void)
{
    char *argv[] = {"vyt", "-s", "some query", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == 0, "search: expected success");
    CHECK(opts.mode == MODE_SEARCH, "search: expected MODE_SEARCH");
    CHECK(opts.argument != NULL && strcmp(opts.argument, "some query") == 0,
          "search: expected argument to be the query");
}

static void test_help_mode(void)
{
    char *argv[] = {"vyt", "-h", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == 0, "help: expected success");
    CHECK(opts.mode == MODE_HELP, "help: expected MODE_HELP");
}

static void test_version_mode(void)
{
    char *argv[] = {"vyt", "-V", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == 0, "version: expected success");
    CHECK(opts.mode == MODE_VERSION, "version: expected MODE_VERSION");
}

static void test_no_mode_is_error(void)
{
    char *argv[] = {"vyt", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == -1, "no mode: expected failure");
}

static void test_conflicting_modes_is_error(void)
{
    char *argv[] = {"vyt", "-m", "a", "-v", "b", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == -1, "conflicting modes: expected failure");
}

static void test_help_with_other_flag_is_error(void)
{
    char *argv[] = {"vyt", "-h", "-V", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == -1, "help+version: expected failure");
}

static void test_missing_argument_is_error(void)
{
    char *argv[] = {"vyt", "-m", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == -1, "missing argument: expected failure");
}

static void test_unknown_option_is_error(void)
{
    char *argv[] = {"vyt", "-x", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == -1, "unknown option: expected failure");
}

static void test_trailing_positional_is_error(void)
{
    char *argv[] = {"vyt", "-h", "extra", NULL};
    options_t opts;
    int rc = run_parse(argv, &opts);

    CHECK(rc == -1, "trailing positional: expected failure");
}

int main(void)
{
    test_music_mode();
    test_video_mode();
    test_search_mode();
    test_help_mode();
    test_version_mode();
    test_no_mode_is_error();
    test_conflicting_modes_is_error();
    test_help_with_other_flag_is_error();
    test_missing_argument_is_error();
    test_unknown_option_is_error();
    test_trailing_positional_is_error();

    if (failures == 0)
    {
        printf("All %d checks passed.\n", checks);
        return 0;
    }

    fprintf(stderr, "%d/%d checks failed.\n", failures, checks);
    return 1;
}
