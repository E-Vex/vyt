#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cli.h"

void cli_print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage:\n"
            "  vyt -m URL\n"
            "  vyt -v URL\n"
            "  vyt -s QUERY\n"
            "  vyt -h\n");
}

void cli_print_help(void)
{
    cli_print_usage(stdout);
    printf(
        "\n"
        "Options:\n"
        "  -m URL    Play YouTube URL as music (audio only, via mpv --no-video)\n"
        "  -v URL    Play YouTube URL as video (via mpv)\n"
        "  -s QUERY  Search YouTube for QUERY (via yt-dlp, top 10 results)\n"
        "  -h        Show this message :D\n"
        "\n"
        "vyt requires yt-dlp and mpv.\n"
        "Make sure you have the latest versions installed and available on your PATH..\n");
}

int cli_parse(int argc, char **argv, options_t *opts)
{
    opts->mode = MODE_NONE;
    opts->argument = NULL;

    mode_t mode = MODE_NONE;
    char *argument = NULL;

    opterr = 0;

    int opt;
    while ((opt = getopt(argc, argv, ":m:v:s:h")) != -1)
    {
        switch (opt)
        {
        case 'm':
        case 'v':
        case 's':
            if (mode == MODE_HELP)
            {
                fprintf(stderr, "vyt: error: -h cannot be combined with other options\n");
                return -1;
            }
            if (mode != MODE_NONE)
            {
                fprintf(stderr, "vyt: error: only one of -m, -v or -s may be specified\n");
                return -1;
            }

            if (opt == 'm')
            {
                mode = MODE_MUSIC;
            }
            else if (opt == 'v')
            {
                mode = MODE_VIDEO;
            }
            else
            {
                mode = MODE_SEARCH;
            }

            argument = optarg;
            break;

        case 'h':
            if (mode != MODE_NONE)
            {
                fprintf(stderr, "vyt: error: -h cannot be combined with other options\n");
                return -1;
            }
            mode = MODE_HELP;
            break;

        case ':':
            fprintf(stderr, "vyt: error: option -%c requires an argument\n", optopt);
            return -1;

        case '?':
        default:
            if (optopt)
            {
                fprintf(stderr, "vyt: error: unknown option '-%c'\n", optopt);
            }
            else
            {
                fprintf(stderr, "vyt: error: unknown option '%s'\n", argv[optind - 1]);
            }
            return -1;
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "vyt: error: unexpected argument '%s'\n", argv[optind]);
        return -1;
    }

    if (mode == MODE_NONE)
    {
        fprintf(stderr, "vyt: error: no mode specified (-m, -v, -s or -h)\n");
        return -1;
    }

    opts->mode = mode;
    opts->argument = argument;
    return 0;
}
