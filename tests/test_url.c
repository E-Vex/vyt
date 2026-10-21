#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/url.h"

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

/* The reference is understood and becomes exactly `want`. */
static void expect_url(const char *input, const char *want, const char *msg)
{
    url_ref_status_t status = URL_REF_INVALID;
    char *url = url_normalize_ref(input, &status);

    CHECK(url != NULL, msg);
    if (url != NULL)
    {
        CHECK_STREQ(url, want, msg);
    }
    CHECK(status == URL_REF_OK, msg);
    free(url);
}

/* The reference is rejected with the expected status. */
static void expect_refused(const char *input, url_ref_status_t want,
                            const char *msg)
{
    url_ref_status_t status = URL_REF_INVALID;
    char *url = url_normalize_ref(input, &status);

    CHECK(url == NULL, msg);
    CHECK(status == want, msg);
    free(url);
}

static void test_full_urls(void)
{
    expect_url("https://youtu.be/dQw4w9WgXcQ", "https://youtu.be/dQw4w9WgXcQ",
               "full URL: kept as it is");
    expect_url("http://example.com/video", "http://example.com/video",
               "full URL: http is kept too");
    expect_url("https://www.youtube.com/watch?v=abc",
               "https://www.youtube.com/watch?v=abc",
               "full URL: watch links are kept as they are");
}

static void test_scheme_less_urls(void)
{
    expect_url("youtu.be/dQw4w9WgXcQ", "https://youtu.be/dQw4w9WgXcQ",
               "scheme-less: youtu.be gets a scheme");
    expect_url("youtube.com/watch?v=abc", "https://youtube.com/watch?v=abc",
               "scheme-less: youtube.com gets a scheme");
    expect_url("www.youtube.com/watch?v=abc",
               "https://www.youtube.com/watch?v=abc",
               "scheme-less: www.youtube.com gets a scheme");
    expect_url("m.youtube.com/watch?v=abc",
               "https://m.youtube.com/watch?v=abc",
               "scheme-less: m.youtube.com gets a scheme");
}

static void test_video_ids(void)
{
    expect_url("dQw4w9WgXcQ", "https://youtu.be/dQw4w9WgXcQ",
               "video ID: becomes a youtu.be link");
    expect_url("a-b_cdEFG12", "https://youtu.be/a-b_cdEFG12",
               "video ID: the whole base64url alphabet is allowed");
}

static void test_whitespace_is_ignored(void)
{
    expect_url("  dQw4w9WgXcQ  ", "https://youtu.be/dQw4w9WgXcQ",
               "whitespace: surrounding spaces are ignored");
    expect_url("\thttps://youtu.be/abc\n", "https://youtu.be/abc",
               "whitespace: tabs and newlines are ignored");
}

static void test_empty_references(void)
{
    expect_refused("", URL_REF_EMPTY, "empty: rejected as empty");
    expect_refused("   ", URL_REF_EMPTY, "blank: rejected as empty");
    expect_refused(NULL, URL_REF_EMPTY, "null: rejected as empty");
}

static void test_invalid_references(void)
{
    expect_refused("not a url", URL_REF_INVALID, "invalid: rejected");
    expect_refused("tooshort", URL_REF_INVALID,
                   "invalid: too short to be a video ID");
    expect_refused("waaaaaytoolongid", URL_REF_INVALID,
                   "invalid: too long to be a video ID");
    expect_refused("abc!defghi", URL_REF_INVALID,
                   "invalid: the video ID alphabet is limited");
    expect_refused("ftp://example.com/file", URL_REF_INVALID,
                   "invalid: only http(s) is understood");
    expect_refused("example.com/video", URL_REF_INVALID,
                   "invalid: a non-YouTube host is not guessed at");
}

int main(void)
{
    test_full_urls();
    test_scheme_less_urls();
    test_video_ids();
    test_whitespace_is_ignored();
    test_empty_references();
    test_invalid_references();

    if (failures == 0)
    {
        printf("All %d checks passed.\n", checks);
        return 0;
    }

    fprintf(stderr, "%d/%d checks failed.\n", failures, checks);
    return 1;
}
