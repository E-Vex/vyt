#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/proc.h"

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

/* ------------------------------------------------------------------------ */
/* capture helpers                                                           */
/* ------------------------------------------------------------------------ */

typedef struct
{
    char **lines;
    size_t count;
    size_t capacity;
} line_vec_t;

static void collect_line(const char *line, void *user_data)
{
    line_vec_t *v = user_data;
    if (v->count == v->capacity)
    {
        size_t nc = (v->capacity == 0) ? 8 : v->capacity * 2;
        char **grown = realloc(v->lines, nc * sizeof *grown);
        if (grown == NULL)
        {
            return;
        }
        v->lines = grown;
        v->capacity = nc;
    }
    v->lines[v->count] = strdup(line);
    v->count++;
}

static void vec_free(line_vec_t *v)
{
    for (size_t i = 0; i < v->count; i++)
    {
        free(v->lines[i]);
    }
    free(v->lines);
    v->lines = NULL;
    v->count = 0;
    v->capacity = 0;
}

/* Redirect the real stderr to a temp file for the duration of `fn`, then
 * return the captured text (caller frees).  Used to assert on diagnostics
 * that proc.c writes to stderr. */
typedef void (*err_fn)(void *user);

static char *capture_stderr(err_fn fn, void *user)
{
    fflush(stderr);
    int saved = dup(STDERR_FILENO);
    FILE *tmp = tmpfile();
    CHECK(tmp != NULL, "stderr capture: tmpfile created");
    if (tmp == NULL)
    {
        return NULL;
    }
    dup2(fileno(tmp), STDERR_FILENO);
    fn(user);
    fflush(stderr);
    dup2(saved, STDERR_FILENO);
    close(saved);

    long size = ftell(tmp);
    rewind(tmp);
    char *buf = malloc((size_t)size + 1);
    if (buf != NULL)
    {
        size_t got = fread(buf, 1, (size_t)size, tmp);
        buf[got] = '\0';
    }
    fclose(tmp);
    return buf;
}

/* ------------------------------------------------------------------------ */
/* proc_run                                                                  */
/* ------------------------------------------------------------------------ */

static void test_proc_run_status(void)
{
    char *true_argv[] = {"true", NULL};
    CHECK(proc_run("true", true_argv) == 0, "run: /bin/true exits 0");

    char *false_argv[] = {"false", NULL};
    CHECK(proc_run("false", false_argv) == 1, "run: /bin/false exits 1");

    char *sh_argv[] = {"sh", "-c", "exit 3", NULL};
    CHECK(proc_run("sh", sh_argv) == 3, "run: custom exit status passes through");

    CHECK(proc_run("vyt-no-such-binary-xyz", sh_argv) == 127,
          "run: missing binary reports 127");
}

/* Run the program named by argv[0] with the given argument vector.  argv[0]
 * must match the program name, mirroring the execvp() convention that
 * proc_run() itself follows (proc_run("sh", {"sh", "-c", ...})). */
static void run_child(void *user)
{
    char **argv = user;
    proc_run(argv[0], argv);
}

static void test_proc_run_signals(void)
{
    /* SIGTERM is a "normal" stop: quiet, conventional 128+sig. */
    char *term_argv[] = {"sh", "-c", "kill -TERM $$", NULL};
    CHECK(proc_run("sh", term_argv) == 128 + SIGTERM,
          "run: SIGTERM yields 128+signal");

    /* SIGINT ditto. */
    char *int_argv[] = {"sh", "-c", "kill -INT $$", NULL};
    CHECK(proc_run("sh", int_argv) == 128 + SIGINT,
          "run: SIGINT yields 128+signal");

    /* An unusual signal still reports, without pretending to be an exit. */
    char *hup_argv[] = {"sh", "-c", "kill -HUP $$", NULL};
    int rc = proc_run("sh", hup_argv);
    CHECK(rc == 128 + SIGHUP, "run: SIGHUP yields 128+signal");
}

static void test_proc_run_no_diagnositics_on_normal_paths(void)
{
    /* A clean run and a clean Ctrl+C stop must not print anything. */
    char *term_argv[] = {"sh", "-c", "kill -TERM $$", NULL};
    char *err = capture_stderr(run_child, term_argv);
    CHECK(err != NULL && err[0] == '\0',
          "run: SIGTERM path stays silent (no scary error)");
    free(err);

    char *true_argv[] = {"true", NULL};
    err = capture_stderr(run_child, true_argv);
    CHECK(err != NULL && err[0] == '\0', "run: clean exit stays silent");
    free(err);
}

/* ------------------------------------------------------------------------ */
/* proc_run_capture                                                          */
/* ------------------------------------------------------------------------ */

static void test_capture_basic(void)
{
    line_vec_t v = {0};
    char *argv[] = {"echo", "hello", NULL};
    int rc = proc_run_capture("echo", argv, collect_line, &v);
    CHECK(rc == 0, "capture: echo exits 0");
    CHECK(v.count == 1, "capture: one line collected");
    CHECK(v.count == 1 && strcmp(v.lines[0], "hello") == 0,
          "capture: line content without newline");
    vec_free(&v);
}

static void test_capture_multiple_lines(void)
{
    line_vec_t v = {0};
    char *argv[] = {"sh", "-c", "printf 'a\\nb\\nc\\n'", NULL};
    int rc = proc_run_capture("sh", argv, collect_line, &v);
    CHECK(rc == 0, "capture: sh exits 0");
    CHECK(v.count == 3, "capture: three lines collected");
    CHECK(v.count == 3 && strcmp(v.lines[0], "a") == 0 &&
              strcmp(v.lines[1], "b") == 0 && strcmp(v.lines[2], "c") == 0,
          "capture: lines arrive in order");
    vec_free(&v);
}

static void test_capture_no_trailing_newline(void)
{
    line_vec_t v = {0};
    char *argv[] = {"sh", "-c", "printf 'final'", NULL};
    CHECK(proc_run_capture("sh", argv, collect_line, &v) == 0,
          "capture: no-newline output exits 0");
    CHECK(v.count == 1 && strcmp(v.lines[0], "final") == 0,
          "capture: final line without newline is delivered");
    vec_free(&v);
}

static void test_capture_empty_output(void)
{
    line_vec_t v = {0};
    char *argv[] = {"true", NULL};
    CHECK(proc_run_capture("true", argv, collect_line, &v) == 0,
          "capture: silent child exits 0");
    CHECK(v.count == 0, "capture: no lines from silent child");
    vec_free(&v);
}

static void test_capture_exit_status(void)
{
    line_vec_t v = {0};
    char *argv[] = {"sh", "-c", "echo out; exit 4", NULL};
    CHECK(proc_run_capture("sh", argv, collect_line, &v) == 4,
          "capture: child exit status returned");
    CHECK(v.count == 1, "capture: stdout still collected on failure");
    vec_free(&v);
}

static void run_capture_missing(void *user)
{
    (void)user;
    char *argv[] = {"vyt-no-such-binary-xyz", NULL};
    proc_run_capture("vyt-no-such-binary-xyz", argv, collect_line, NULL);
}

static void test_capture_missing_binary(void)
{
    line_vec_t v = {0};
    char *argv[] = {"vyt-no-such-binary-xyz", NULL};
    int rc = proc_run_capture("vyt-no-such-binary-xyz", argv, collect_line,
                              &v);
    CHECK(rc == 127, "capture: missing binary reports 127");
    CHECK(v.count == 0, "capture: no output from missing binary");
    vec_free(&v);

    /* The not-found diagnostic is the user-facing explanation. */
    char *err = capture_stderr(run_capture_missing, NULL);
    CHECK(err != NULL && strstr(err, "not found") != NULL,
          "capture: missing binary explains itself on stderr");
    free(err);
}

static void test_capture_crlf_stripped(void)
{
    line_vec_t v = {0};
    char *argv[] = {"sh", "-c", "printf 'win\\r\\n'", NULL};
    CHECK(proc_run_capture("sh", argv, collect_line, &v) == 0,
          "capture: crlf output exits 0");
    CHECK(v.count == 1 && strcmp(v.lines[0], "win") == 0,
          "capture: trailing CR stripped");
    vec_free(&v);
}

int main(void)
{
    test_proc_run_status();
    test_proc_run_signals();
    test_proc_run_no_diagnositics_on_normal_paths();
    test_capture_basic();
    test_capture_multiple_lines();
    test_capture_no_trailing_newline();
    test_capture_empty_output();
    test_capture_exit_status();
    test_capture_missing_binary();
    test_capture_crlf_stripped();

    if (failures == 0)
    {
        printf("All %d checks passed.\n", checks);
        return 0;
    }

    fprintf(stderr, "%d/%d checks failed.\n", failures, checks);
    return 1;
}
