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
