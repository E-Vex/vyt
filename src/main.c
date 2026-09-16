/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "cli.h"
#include "player.h"
#include "playlist_cmd.h"
#include "search.h"

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        cli_print_usage(stderr);
        return EXIT_FAILURE;
    }

    /* Subcommand form: "vyt playlist ...".
     * Handled before cli_parse so getopt never sees these words.
     * argv + 2 is safe even when argc == 2: argv[argc] is NULL. */
    if (strcmp(argv[1], "playlist") == 0)
    {
        return playlist_cmd_run(argc - 2, argv + 2);
    }

    options_t opts;
    if (cli_parse(argc, argv, &opts) != 0)
    {
        cli_print_usage(stderr);
        return EXIT_FAILURE;
    }

    int status = 0;

    switch (opts.mode)
    {
    case MODE_MUSIC:
        status = player_play_music(opts.argument);
        break;

    case MODE_VIDEO:
        status = player_play_video(opts.argument);
        break;

    case MODE_SEARCH:
        status = search_youtube(opts.argument);
        break;

    case MODE_HELP:
        cli_print_help();
        return EXIT_SUCCESS;

    case MODE_VERSION:
        cli_print_version();
        return EXIT_SUCCESS;

    case MODE_NONE:
    default:
        cli_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (status < 0)
    {
        return EXIT_FAILURE;
    }

    return status;
}
