#ifndef PLAYLIST_CMD_H
#define PLAYLIST_CMD_H

#include <stdio.h>

/* Bare syntax listing (no descriptions), for error paths. */
void playlist_cmd_print_usage(FILE *stream);

/* Described command list, printed by `vyt playlist help`. */
void playlist_cmd_print_help(FILE *stream);

/* Runs the "playlist" subcommand.
 * argc/argv start AFTER the word "playlist":
 *   vyt playlist song add chill URL  ->  argv = {"song","add","chill","URL"}
 * Returns 0 on success, 1 on failure. */
int playlist_cmd_run(int argc, char **argv);

#endif /* PLAYLIST_CMD_H */
