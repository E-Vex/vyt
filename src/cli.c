#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cli.h"
#include "output.h"
#include "version.h"

void cli_print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage:\n"
            "  vyt -m URL\n"
            "  vyt -v URL\n"
            "  vyt -s QUERY\n"
            "  vyt playlist <command> ...\n"
            "  vyt -V\n"
            "  vyt -h\n");
}

void cli_print_help(void)
{
    FILE *out = stdout;
    const char *b = out_sgr(out, "1");
    const char *r = out_reset(out);

    fprintf(out, "%sUsage:%s\n", b, r);
    fputs(
        "  vyt -m URL\n"
        "  vyt -v URL\n"
        "  vyt -s QUERY\n"
        "  vyt playlist <command> ...\n"
        "  vyt -V\n"
        "  vyt -h\n",
        out);

    fprintf(out, "\n%sOptions:%s\n", b, r);
    fputs(
        "  -m URL    Play YouTube URL as music (audio only, via mpv --no-video)\n"
        "  -v URL    Play YouTube URL as video (via mpv)\n"
        "  -s QUERY  Search YouTube for QUERY (via yt-dlp, top 10 results)\n"
        "  -V        Print the version\n"
        "  -h        Show this message :D\n",
        out);

    fprintf(out, "\n%sPlaylist:%s\n", b, r);
    fputs(
        "  vyt playlist add <name>                          Create a new playlist\n"
        "  vyt playlist list                                List all playlists\n"
        "  vyt playlist remove <name>                       Remove a playlist\n"
        "  vyt playlist play <name>                         Play a playlist shuffled forever\n"
        "  vyt playlist song add <p> <song_name> <url>      Add a named song to a playlist\n"
        "  vyt playlist song remove <p> <url>               Remove a URL from a playlist\n"
        "  vyt playlist song list <p>                       List songs in a playlist\n",
        out);

    fprintf(out,
        "\n%sSearch results and song lists are numbered on a terminal;"
        " when stdout is\nredirected they fall back to a stable plain"
        " format for scripts.%s\n",
        out_sgr(out, "2"), out_reset(out));

    fprintf(out,
        "\nvyt requires yt-dlp and mpv.\n"
        "Make sure you have the latest versions installed and available on your PATH.\n");
}

void cli_print_version(void)
{
    printf("vyt %s\n", VYT_VERSION);
}

int cli_parse(int argc, char **argv, options_t *opts)
{
    opts->mode = MODE_NONE;
    opts->argument = NULL;

    /*
     * Pre-dispatch: `vyt playlist ...` is a positional subcommand tree.
     * getopt() cannot express `playlist add <name>`, so we intercept it
     * here, before getopt permutes argv, and let main.c delegate the
     * remaining argv to the playlist module.
     */
    if (argc >= 2 && strcmp(argv[1], "playlist") == 0)
    {
        opts->mode = MODE_PLAYLIST;
        opts->argument = NULL;
        return 0;
    }

    cli_mode_t mode = MODE_NONE;
    char *argument = NULL;

    opterr = 0;

    int opt;
    while ((opt = getopt(argc, argv, ":m:v:s:hV")) != -1)
    {
        switch (opt)
        {
        case 'm':
        case 'v':
        case 's':
            if (mode == MODE_HELP || mode == MODE_VERSION)
            {
                out_error(stderr, "-h/-V cannot be combined with other options");
                return -1;
            }
            if (mode != MODE_NONE)
            {
                out_error(stderr, "only one of -m, -v or -s may be specified");
                return -1;
            }
            if (optarg[0] == '\0')
            {
                out_error(stderr, "option -%c requires a non-empty argument", opt);
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
        case 'V':
            if (mode != MODE_NONE)
            {
                out_error(stderr, "-h/-V cannot be combined with other options");
                return -1;
            }

            if (opt == 'h')
            {
                mode = MODE_HELP;
            }
            else
            {
                mode = MODE_VERSION;
            }
            break;

        case ':':
            out_error(stderr, "option -%c requires an argument", optopt);
            return -1;

        case '?':
        default:
            if (optopt)
            {
                out_error(stderr, "unknown option '-%c'", optopt);
            }
            else
            {
                out_error(stderr, "unknown option '%s'", argv[optind - 1]);
            }
            return -1;
        }
    }

    if (optind < argc)
    {
        out_error(stderr, "unexpected argument '%s'", argv[optind]);
        return -1;
    }

    if (mode == MODE_NONE)
    {
        out_error(stderr, "no mode specified (-m, -v, -s or -h)");
        return -1;
    }

    opts->mode = mode;
    opts->argument = argument;
    return 0;
}
