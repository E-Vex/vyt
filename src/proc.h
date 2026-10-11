#ifndef PROC_H
#define PROC_H

#include <stddef.h>

/*
 * Callback used by proc_run_capture().  It receives one line of the
 * child's stdout at a time, without the trailing newline, and `user_data`
 * as given by the caller.  Lines are delivered in order; after the last
 * line the callback is not called again.
 */
typedef void (*proc_line_fn)(const char *line, void *user_data);

/*
 * Run `program` with `argv` (execvp-style, argv[0] first, NULL-terminated)
 * and wait for it.  The child inherits stdin, stdout and stderr.
 *
 * Returns the child's exit status.  If the child was killed by SIGINT or
 * SIGTERM the conventional 128+signal is returned silently; any other
 * signal prints a diagnostic and still returns 128+signal.  Returns -1 if
 * vyt itself could not fork or wait (a diagnostic has been printed).
 */
int proc_run(const char *program, char *const argv[]);

/*
 * Run `program` like proc_run(), but redirect the child's stdout into a
 * pipe and deliver it line by line to `on_line`.  The child's stderr is
 * inherited untouched, so tools like yt-dlp keep reporting their own
 * diagnostics straight to the user.
 *
 * Lines arrive without their trailing newline (a trailing CR is also
 * stripped).  All lines are consumed before the child is reaped, so the
 * function only returns once the child has exited.
 *
 * Return value semantics are identical to proc_run().
 */
int proc_run_capture(const char *program, char *const argv[],
                     proc_line_fn on_line, void *user_data);

#endif /* PROC_H */
