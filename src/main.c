/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <stdlib.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "cli.h"
#include "player.h"
#include "search.h"

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        cli_print_usage(stderr);
        return EXIT_FAILURE;
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
