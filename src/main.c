/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <stdlib.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "cli.h"

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

    switch (opts.mode)
    {
    case MODE_MUSIC:
        /* TODO: invoke yt-dlp/mpv for music playback */
        return EXIT_SUCCESS;
    case MODE_VIDEO:
        /* TODO: invoke yt-dlp/mpv for video playback */
        return EXIT_SUCCESS;
    case MODE_SEARCH:
        /* TODO: invoke yt-dlp search */
        return EXIT_SUCCESS;
    case MODE_HELP:
        cli_print_help();
        return EXIT_SUCCESS;
    case MODE_NONE:
    default:
        cli_print_usage(stderr);
        return EXIT_FAILURE;
    }
}
