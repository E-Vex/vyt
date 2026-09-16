#define _POSIX_C_SOURCE 200809L

/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "playlist.h"
#include "playlist_cmd.h"

/* ========================================================================= */
/* OUTPUT HELPERS                                                            */
/* ========================================================================= */

void playlist_cmd_print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage:\n"
            "  vyt playlist add <name>\n"
            "  vyt playlist list\n"
            "  vyt playlist remove <name>\n"
            "  vyt playlist play <name>\n"
            "  vyt playlist song add <playlist> <url>\n"
            "  vyt playlist song remove <playlist> <url>\n"
            "  vyt playlist song list <playlist>\n");
}

/* Reports a failed storage operation and returns the shell exit code.
 * Shape:  vyt: error: chill: playlist already exists
 * which matches how rm/cp report a failing subject. */
static int report(const char *subject, pl_status_t status)
{
    const char *detail = (status == PL_ERR_IO) ? strerror(errno)
                                               : playlist_strerror(status);

    if (subject == NULL)
    {
        fprintf(stderr, "vyt: error: %s\n", detail);
    }
    else
    {
        fprintf(stderr, "vyt: error: %s: %s\n", subject, detail);
    }

    return 1;
}

/* Wrong number of words after "vyt playlist ...". */
static int usage_error(const char *command, const char *expected)
{
    fprintf(stderr, "vyt: error: '%s' takes %s\n", command, expected);
    playlist_cmd_print_usage(stderr);
    return 1;
}

/* ========================================================================= */
/* COMMANDS                                                                  */
/* ========================================================================= */

static int cmd_add(int argc, char **argv)
{
    if (argc != 1)
    {
        return usage_error("playlist add", "exactly one playlist name");
    }

    pl_status_t status = playlist_create(argv[0]);
    if (status != PL_OK)
    {
        return report(argv[0], status);
    }

    printf("created playlist '%s'\n", argv[0]);
    return 0;
}

static int cmd_remove(int argc, char **argv)
{
    if (argc != 1)
    {
        return usage_error("playlist remove", "exactly one playlist name");
    }

    pl_status_t status = playlist_delete(argv[0]);
    if (status != PL_OK)
    {
        return report(argv[0], status);
    }

    printf("removed playlist '%s'\n", argv[0]);
    return 0;
}

static int cmd_list(int argc, char **argv)
{
    (void)argv;

    if (argc != 0)
    {
        return usage_error("playlist list", "no arguments");
    }

    char **names = NULL;
    size_t count = 0;

    pl_status_t status = playlist_list(&names, &count);
    if (status != PL_OK)
    {
        return report(NULL, status);
    }

    if (count == 0)
    {
        /* Not an error: an empty collection is a valid answer. */
        fprintf(stderr, "vyt: no playlists yet (create one with 'vyt playlist add <name>')\n");
        playlist_list_free(names, count);
        return 0;
    }

    for (size_t i = 0; i < count; i++)
    {
        printf("%s\n", names[i]);
    }

    playlist_list_free(names, count);
    return 0;
}

/* ========================================================================= */
/* DISPATCH                                                                  */
/* ========================================================================= */

int playlist_cmd_run(int argc, char **argv)
{
    if (argc < 1)
    {
        fprintf(stderr, "vyt: error: 'playlist' requires a command\n");
        playlist_cmd_print_usage(stderr);
        return 1;
    }

    const char *command = argv[0];

    if (strcmp(command, "add") == 0)
    {
        return cmd_add(argc - 1, argv + 1);
    }

    if (strcmp(command, "list") == 0)
    {
        return cmd_list(argc - 1, argv + 1);
    }

    if (strcmp(command, "remove") == 0)
    {
        return cmd_remove(argc - 1, argv + 1);
    }

    if (strcmp(command, "help") == 0 || strcmp(command, "-h") == 0)
    {
        playlist_cmd_print_usage(stdout);
        return 0;
    }

    /* TODO(stage 3): song add/remove/list.  TODO(stage 4): play. */
    if (strcmp(command, "song") == 0 || strcmp(command, "play") == 0)
    {
        fprintf(stderr, "vyt: error: 'playlist %s' is not implemented yet\n", command);
        return 1;
    }

    fprintf(stderr, "vyt: error: unknown playlist command '%s'\n", command);
    playlist_cmd_print_usage(stderr);
    return 1;
}