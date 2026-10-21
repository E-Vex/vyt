#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE /* getopt_long() */

/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "cli.h"
#include "output.h"
#include "url.h"
#include "version.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/*
 * The long spellings of every option.  The short forms stay the documented
 * way to use vyt; the long ones are there for people who type them out of
 * habit, and for scripts that read better with them.
 */
static const struct option long_options[] = {
    {"music", required_argument, NULL, 'm'},
    {"video", required_argument, NULL, 'v'},
    {"search", required_argument, NULL, 's'},
    {"download", required_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0},
};

/* ========================================================================= */
/* HELP TEXT                                                                 */
/* ========================================================================= */

void cli_print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage:\n"
            "  vyt -m URL\n"
            "  vyt -v URL\n"
            "  vyt -s QUERY\n"
            "  vyt -d URL\n"
            "  vyt playlist <command> ...\n"
            "  vyt -V\n"
            "  vyt -h\n");
}

void cli_print_help(void)
{
    FILE *out = stdout;

    fprintf(out, "%sUsage:%s\n", out_style(out, OUT_BOLD),
            out_style(out, OUT_RESET));
    fputs(
        "  vyt -m URL\n"
        "  vyt -v URL\n"
        "  vyt -s QUERY\n"
        "  vyt -d URL\n"
        "  vyt playlist <command> ...\n"
        "  vyt -V\n"
        "  vyt -h\n",
        out);

    fprintf(out, "\n%sOptions:%s\n", out_style(out, OUT_BOLD),
            out_style(out, OUT_RESET));
    fputs(
        "  -m, --music URL     Play YouTube URL as music (audio only, via mpv --no-video)\n"
        "  -v, --video URL     Play YouTube URL as video (via mpv)\n"
        "  -s, --search QUERY  Search YouTube for QUERY (via yt-dlp, top 10 results)\n"
        "  -d, --download URL  Download YouTube media via yt-dlp to the current directory\n"
        "  -V, --version       Print the version\n"
        "  -h, --help          Show this message :D\n",
        out);

    fprintf(out, "\n%sPlaylist:%s\n", out_style(out, OUT_BOLD),
            out_style(out, OUT_RESET));
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
            "\n%sURLs may be full links or bare video IDs: both"
            " 'vyt -m dQw4w9WgXcQ' and"
            " 'vyt -m https://youtu.be/dQw4w9WgXcQ' play the same video.%s\n",
            out_style(out, OUT_DIM), out_style(out, OUT_RESET));

    fprintf(out,
            "\n%sSearch results and song lists are numbered on a terminal;"
            " when stdout is\nredirected they fall back to a stable plain"
            " format for scripts.%s\n",
            out_style(out, OUT_DIM), out_style(out, OUT_RESET));

    fprintf(out,
            "\nvyt requires yt-dlp and mpv.\n"
            "Make sure you have the latest versions installed and available on your PATH.\n");
}

void cli_print_version(void)
{
    printf("vyt %s\n", VYT_VERSION);
}

/* ========================================================================= */
/* ARGUMENT PARSING                                                          */
/* ========================================================================= */

/*
 * The option as the user spelled it: "--music" for a long option, "-m" for
 * a short one.  getopt_long reports which long option it matched, so the
 * error messages can quote the user's own wording back at them.
 *
 * The result lives in a single static buffer: use it right away.
 */
static const char *option_name(int opt, int longindex)
{
    static char buf[32];

    if (longindex >= 0 && (size_t)longindex < COUNT(long_options) &&
        long_options[longindex].name != NULL)
    {
        snprintf(buf, sizeof buf, "--%s", long_options[longindex].name);
        return buf;
    }

    snprintf(buf, sizeof buf, "-%c", opt);
    return buf;
}

/* The word the user typed for an option getopt could not use: it is the
 * last word getopt looked at. */
static const char *typed_option_name(int opt, int longindex, char **argv)
{
    const char *typed = argv[optind - 1];
    if (typed != NULL && typed[0] == '-' && typed[1] != '\0')
    {
        return typed;
    }
    return option_name(opt, longindex);
}

/*
 * Turn the option's argument into the value the mode works with.
 *
 * A search query is free text and is kept as it was typed; every other
 * mode wants a media reference, which is normalized into a URL mpv and
 * yt-dlp can open.
 */
static char *argument_for_mode(cli_mode_t mode, const char *raw, int opt,
                                int longindex)
{
    if (raw[0] == '\0')
    {
        out_error(stderr, "option %s requires a non-empty argument",
                  option_name(opt, longindex));
        return NULL;
    }

    if (mode == MODE_SEARCH)
    {
        char *copy = strdup(raw);
        if (copy == NULL)
        {
            out_error(stderr, "out of memory");
        }
        return copy;
    }

    url_ref_status_t status = URL_REF_INVALID;
    char *url = url_normalize_ref(raw, &status);

    if (url == NULL)
    {
        if (status == URL_REF_EMPTY)
        {
            out_error(stderr, "option %s requires a non-empty argument",
                      option_name(opt, longindex));
        }
        else
        {
            out_error(stderr, "'%s' is not a YouTube URL or video ID", raw);
            out_hint(stderr,
                     "expected a link such as"
                     " https://youtu.be/dQw4w9WgXcQ, or a video ID");
        }
    }
    return url;
}

int cli_parse(int argc, char **argv, options_t *opts)
{
    opts->mode = MODE_NONE;
    opts->argument = NULL;

    cli_mode_t mode = MODE_NONE;
    const char *raw = NULL; /* the option's argument, still as it was typed */
    int mode_opt = 0;       /* the option that selected the mode */
    int mode_longindex = -1;

    opterr = 0;

    int opt;
    int longindex = -1;
    while ((opt = getopt_long(argc, argv, ":m:v:s:d:hV", long_options,
                               &longindex)) != -1)
    {
        switch (opt)
        {
        case 'm':
        case 'v':
        case 's':
        case 'd':
            if (mode == MODE_HELP || mode == MODE_VERSION)
            {
                out_error(stderr, "-h/-V cannot be combined with other options");
                return -1;
            }
            if (mode != MODE_NONE)
            {
                out_error(stderr,
                          "only one of -m, -v, -s or -d may be specified");
                return -1;
            }

            switch (opt)
            {
            case 'm':
                mode = MODE_MUSIC;
                break;
            case 'v':
                mode = MODE_VIDEO;
                break;
            case 's':
                mode = MODE_SEARCH;
                break;
            default:
                mode = MODE_DOWNLOAD;
                break;
            }

            raw = optarg;
            mode_opt = opt;
            mode_longindex = longindex;
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
            out_error(stderr, "option %s requires an argument",
                      typed_option_name(optopt, longindex, argv));
            return -1;

        case '?':
        default:
            out_error(stderr, "unknown option '%s'",
                      typed_option_name(opt, longindex, argv));
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
        out_error(stderr, "no mode specified (-m, -v, -s or -d)");
        out_hint(stderr, "run 'vyt --help' to see everything vyt can do");
        return -1;
    }

    /* The command line made sense, so now the argument can be checked:
     * an unusable reference is only worth reporting once we know which
     * mode it was meant for. */
    if (raw != NULL)
    {
        opts->argument = argument_for_mode(mode, raw, mode_opt,
                                           mode_longindex);
        if (opts->argument == NULL)
        {
            return -1;
        }
    }

    opts->mode = mode;
    return 0;
}
