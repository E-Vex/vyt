#ifndef PLAYLIST_CMD_H
#define PLAYLIST_CMD_H

/* Runs the "playlist" subcommand.
 * argc/argv start AFTER the word "playlist":
 *   vyt playlist song add chill URL  ->  argv = {"song","add","chill","URL"}
 * Returns 0 on success, 1 on failure. */
int playlist_cmd_run(int argc, char **argv);

#endif /* PLAYLIST_CMD_H */
