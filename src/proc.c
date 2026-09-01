#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int proc_run(const char *program, char *const argv[])
{

    pid_t pid = fork();

    if (pid < 0)
    {
        fprintf(stderr, "vyt: error: failed to create process: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0)
    {
        /* Child: replace this process image with the target program.
         * execvp searches PATH and never returns on success. */
        execvp(program, argv);

        if (errno == ENOENT)
        {
            fprintf(stderr, "vyt: error: '%s' not found (is it installed and on PATH?)\n", program);
        }
        else
        {
            fprintf(stderr, "vyt: error: failed to execute '%s': %s\n", program, strerror(errno));
        }
        _exit(127);
    }

    /* Parent: wait for the child to finish. */
    int status;
    if (waitpid(pid, &status, 0) < 0)
    {
        fprintf(stderr, "vyt: error: failed to wait for '%s': %s\n", program, strerror(errno));
        return -1;
    }

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status))
    {
        fprintf(stderr, "vyt: error: '%s' was terminated by signal %d (%s)\n", program, WTERMSIG(status), strsignal(WTERMSIG(status)));
        return -2;
    }

    return -1;
}
