#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Common test framework macros and helpers. */

extern int test_failures;
extern int test_checks;

#define TEST_CHECK(cond, msg)                                                    \
    do                                                                           \
    {                                                                            \
        test_checks++;                                                           \
        if (!(cond))                                                             \
        {                                                                        \
            test_failures++;                                                     \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);      \
        }                                                                        \
    } while (0)

#define TEST_CHECK_STREQ(got, want, msg)                                         \
    do                                                                           \
    {                                                                            \
        const char *g_ = (got);                                                  \
        const char *w_ = (want);                                                 \
        test_checks++;                                                           \
        if (g_ == NULL || strcmp(g_, w_) != 0)                                   \
        {                                                                        \
            test_failures++;                                                     \
            fprintf(stderr, "FAIL: %s (got \"%s\", want \"%s\") (%s:%d)\n",      \
                    msg, g_ ? g_ : "(null)", w_, __FILE__, __LINE__);            \
        }                                                                        \
    } while (0)

/* Runs `fn` with an open_memstream; returns the collected text (caller frees). */
typedef void (*test_sink_fn)(FILE *out, void *user);

static inline char *test_capture(test_sink_fn fn, void *user)
{
    char *buf = NULL;
    size_t len = 0;
    FILE *ms = open_memstream(&buf, &len);
    if (ms == NULL)
    {
        return NULL;
    }
    fn(ms, user);
    fclose(ms);
    return buf;
}

static inline int test_report(void)
{
    if (test_failures == 0)
    {
        printf("All %d checks passed.\n", test_checks);
        return 0;
    }

    fprintf(stderr, "%d/%d checks failed.\n", test_failures, test_checks);
    return 1;
}

#endif /* TEST_HELPERS_H */