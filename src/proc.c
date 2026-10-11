#define _POSIX_C_SOURCE 200809L

/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "output.h"
#include "proc.h"

/* ========================================================================= */
/* INTERNALS                                                                 */
/* ========================================================================= */

/*
 * Child side of both runners: replace the process image with `program`.
 * Never returns; exits 127 (the conventional "command not found" status)
 * if execvp fails, after printing a diagnostic the user can act on.
 */
static void exec_or_die(const char *program, char *const argv[])
{
    execvp(program, argv);

    /* execvp only returns on failure.  ENOENT is worth its own message:
     * a missing yt-dlp/mpv is by far the most common failure. */
    if (errno == ENOENT)
    {
        out_error(stderr, "'%s' not found (is it installed and on PATH?)",
                  program);
    }
    else
    {
        out_error(stderr, "failed to execute '%s': %s", program,
                  strerror(errno));
    }
    _exit(127);
}

/*
 * Parent side of both runners: wait for the child and translate the raw
 * wait status into the conventions vyt returns to main().
 *
 * waitpid() can return -1 with errno==EINTR if a signal arrives during
 * the wait.  This is normal and recoverable: just retry.  Treating it as
 * a hard error (the previous behavior) would leak "failed to wait for
 * 'mpv': Interrupted system call" to the user whenever they pressed
 * Ctrl+C during playback.
 */
static int reap_child(pid_t pid, const char *program)
{
    int status;
    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno == EINTR)
        {
            continue;
        }
        out_error(stderr, "failed to wait for '%s': %s", program,
                  strerror(errno));
        return -1;
    }

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status))
    {
        int sig = WTERMSIG(status);

        /* SIGINT/SIGTERM are the normal ways a user stops playback (Ctrl+C
         * delivers SIGINT to the whole foreground process group, mpv dies,
         * and we end up here).  Reporting that as a scary "terminated by
         * signal" error turns a routine action into a failure message, so
         * stay quiet and follow the conventional 128+signal exit status. */
        if (sig == SIGINT || sig == SIGTERM)
        {
            return 128 + sig;
        }

        out_error(stderr, "'%s' was terminated by signal %d (%s)", program,
                  sig, strsignal(sig));
        return 128 + sig;
    }

    return -1;
}

/* ========================================================================= */
/* PUBLIC API                                                                */
/* ========================================================================= */

int proc_run(const char *program, char *const argv[])
{
    pid_t pid = fork();

    if (pid < 0)
    {
        out_error(stderr, "failed to create process: %s", strerror(errno));
        return -1;
    }

    if (pid == 0)
    {
        /* Child: replace this process image with the target program.
         * execvp searches PATH and never returns on success. */
        exec_or_die(program, argv);
    }

    /* Parent: wait for the child to finish. */
    return reap_child(pid, program);
}

int proc_run_capture(const char *program, char *const argv[],
                     proc_line_fn on_line, void *user_data)
{
    int fds[2];
    if (pipe(fds) != 0)
    {
        out_error(stderr, "failed to create pipe: %s", strerror(errno));
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        out_error(stderr, "failed to create process: %s", strerror(errno));
        close(fds[0]);
        close(fds[1]);
        return -1;
    }

    if (pid == 0)
    {
        /* Child: stdout becomes the pipe; stderr stays inherited so the
         * child's own diagnostics still reach the user. */
        close(fds[0]);
        if (dup2(fds[1], STDOUT_FILENO) != STDOUT_FILENO)
        {
            out_error(stderr, "failed to redirect '%s' output: %s", program,
                      strerror(errno));
            _exit(126);
        }
        close(fds[1]);
        exec_or_die(program, argv);
    }

    /* Parent: read every line the child writes before reaping it. */
    close(fds[1]);

    FILE *fp = fdopen(fds[0], "r");
    if (fp == NULL)
    {
        close(fds[0]);
        reap_child(pid, program);
        return -1;
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    while ((n = getline(&line, &cap, fp)) != -1)
    {
        if (n > 0 && line[n - 1] == '\n')
        {
            line[--n] = '\0';
        }
        if (n > 0 && line[n - 1] == '\r')
        {
            line[n - 1] = '\0';
        }
        on_line(line, user_data);
    }

    free(line);
    fclose(fp);

    return reap_child(pid, program);
}
