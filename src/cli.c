#include <stdio.h>

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

    /* TODO: Parse command-line arguments and populate opts->mode and opts->argument */

    return 0;
}
