#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE /* getrandom() on older glibc */

/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "output.h"
#include "player.h"
#include "playlist.h"
#include "playlist_cmd.h"

/* ========================================================================= */
/* USAGE TEXT                                                                */
/* ========================================================================= */

/* Bare syntax, no descriptions: what error paths print. */
void playlist_cmd_print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage:\n"
            "  vyt playlist add <name>\n"
            "  vyt playlist list\n"
            "  vyt playlist remove <name>\n"
            "  vyt playlist play <name>\n"
            "  vyt playlist song add <playlist> <song_name> <url>\n"
            "  vyt playlist song remove <playlist> <url>\n"
            "  vyt playlist song list <playlist>\n");
}

/* Described command list: what `vyt playlist help` prints. */
void playlist_cmd_print_help(FILE *stream)
{
    fprintf(stream,
            "Usage:\n"
            "  vyt playlist add <name>                           Create a new playlist\n"
            "  vyt playlist list                                 List all playlists\n"
            "  vyt playlist remove <name>                        Remove a playlist\n"
            "  vyt playlist play <name>                          Play a playlist shuffled forever\n"
            "  vyt playlist song add <playlist> <song_name> <url>     Add a named song to a playlist\n"
            "  vyt playlist song remove <playlist> <url>              Remove a song by URL\n"
            "  vyt playlist song list <playlist>                      List the songs in a playlist\n");
}

/* The three song subcommand lines, for errors inside `playlist song ...`. */
static void print_song_usage(FILE *stream)
{
    fputs("Usage:\n"
          "  vyt playlist song add <playlist> <song_name> <url>\n"
          "  vyt playlist song remove <playlist> <url>\n"
          "  vyt playlist song list <playlist>\n",
          stream);
}

/* ========================================================================= */
/* ERROR REPORTING                                                           */
/* ========================================================================= */

/*
 * Report a failed playlist operation and return the shell exit code.
 *
 * Every failure gets the same shape: one `vyt: error:` line naming what
 * failed and why, plus -- whenever a concrete next step exists -- one
 * `vyt: hint:` line with a command the user can copy and paste.
 *
 * `action` describes the operation for filesystem errors ("create",
 * "add a song to", ...); `playlist`, `song_name` and `url` carry whatever
 * context the caller has (NULL where not applicable).
 */
static int fail_ctx(const char *action, const char *playlist,
                    const char *song_name, const char *url,
                    pl_status_t status)
{
    switch (status)
    {
    case PL_ERR_NOT_FOUND:
        out_error(stderr, "no playlist named '%s'", playlist);
        out_hint(stderr, "run 'vyt playlist list' to see your playlists");
        break;

    case PL_ERR_EXISTS:
        out_error(stderr, "cannot create '%s': playlist already exists",
                  playlist);
        out_hint(stderr, "remove it first with 'vyt playlist remove %s'",
                 playlist);
        break;

    case PL_ERR_DUPLICATE:
        out_error(stderr, "cannot add to '%s': %s is already in the playlist",
                  playlist, url);
        out_hint(stderr, "remove it first with 'vyt playlist song remove %s %s'",
                 playlist, url);
        break;

    case PL_ERR_NO_SONG:
        out_error(stderr,
                  "cannot remove from '%s': %s is not in the playlist",
                  playlist, url);
        out_hint(stderr,
                 "run 'vyt playlist song list %s' to see its songs",
                 playlist);
        break;

    case PL_ERR_NAME:
        if (playlist != NULL)
        {
            out_error(stderr, "invalid playlist name '%s'", playlist);
        }
        else
        {
            out_error(stderr, "invalid playlist name");
        }
        out_hint(stderr,
                 "names may not contain '/', control characters, leading dots,"
                 " or leading/trailing blanks");
        break;

    case PL_ERR_SONG_NAME:
        out_error(stderr, "invalid song name '%s'",
                  (song_name != NULL) ? song_name : "");
        out_hint(stderr,
                 "song names must be 1-256 bytes and may not contain"
                 " control characters");
        break;

    case PL_ERR_URL:
        out_error(stderr, "invalid URL '%s'", (url != NULL) ? url : "");
        out_hint(stderr, "URLs must start with http:// or https://");
        break;

    case PL_ERR_CONFIG:
        out_error(stderr, "cannot determine the configuration directory");
        out_hint(stderr,
                 "set HOME (or XDG_CONFIG_HOME) to an absolute path");
        break;

    case PL_ERR_MEMORY:
        out_error(stderr, "out of memory");
        break;

    case PL_ERR_IO:
    default:
        if (playlist != NULL)
        {
            out_error(stderr, "could not %s '%s': %s", action, playlist,
                      strerror(errno));
        }
        else
        {
            out_error(stderr, "could not %s: %s", action, strerror(errno));
        }
        break;
    }

    return 1;
}

/* Wrong number of words after "vyt playlist ...".  The usage line for the
 * offending command replaces the old full-dump, so the fix is one glance
 * away without scrolling past six unrelated commands. */
static int usage_error(const char *command, const char *expected,
                       const char *usage)
{
    out_error(stderr, "'%s' takes %s", command, expected);
    out_hint(stderr, "usage: %s", usage);
    return 1;
}

/* ========================================================================= */
/* COMMANDS                                                                  */
/* ========================================================================= */

static int cmd_add(int argc, char **argv)
{
    if (argc != 1)
    {
        return usage_error("playlist add", "exactly one playlist name",
                           "vyt playlist add <name>");
    }

    pl_status_t status = playlist_create(argv[0]);
    if (status != PL_OK)
    {
        return fail_ctx("create", argv[0], NULL, NULL, status);
    }

    out_ok(stdout, "created playlist '%s'", argv[0]);
    return 0;
}

static int cmd_remove(int argc, char **argv)
{
    if (argc != 1)
    {
        return usage_error("playlist remove", "exactly one playlist name",
                           "vyt playlist remove <name>");
    }

    pl_status_t status = playlist_delete(argv[0]);
    if (status != PL_OK)
    {
        return fail_ctx("remove", argv[0], NULL, NULL, status);
    }

    out_ok(stdout, "removed playlist '%s'", argv[0]);
    return 0;
}

/* Digit count of n, minimum 1; used to align numbered lists. */
static size_t num_cols(size_t n)
{
    size_t cols = 1;
    while (n >= 10)
    {
        n /= 10;
        cols++;
    }
    return cols;
}

static int cmd_list(int argc, char **argv)
{
    (void)argv;

    if (argc != 0)
    {
        return usage_error("playlist list", "no arguments",
                           "vyt playlist list");
    }

    char **names = NULL;
    size_t count = 0;

    pl_status_t status = playlist_list(&names, &count);
    if (status != PL_OK)
    {
        return fail_ctx("list playlists", NULL, NULL, NULL, status);
    }

    if (count == 0)
    {
        /* Not an error: an empty collection is a valid answer.  The note
         * goes to stderr so stdout stays empty for scripts. */
        out_status(stderr, "no playlists yet");
        out_hint(stderr, "create one with 'vyt playlist add <name>'");
        playlist_list_free(names, count);
        return 0;
    }

    if (!out_stdout_is_tty())
    {
        /* Scriptable mode: one name per line, nothing else. */
        for (size_t i = 0; i < count; i++)
        {
            printf("%s\n", names[i]);
        }
        playlist_list_free(names, count);
        return 0;
    }

    /* Interactive mode: align names in a column and show how many songs
     * each playlist holds.  Loads are local file reads; malformed lines
     * still surface through the usual warnings. */
    size_t *counts = malloc(count * sizeof *counts);
    if (counts == NULL)
    {
        /* Fall back to the plain listing rather than failing the command:
         * we already know the names, which is what was asked for. */
        for (size_t i = 0; i < count; i++)
        {
            printf("%s\n", names[i]);
        }
        playlist_list_free(names, count);
        return 0;
    }

    size_t max_width = 0;
    for (size_t i = 0; i < count; i++)
    {
        playlist_t pl;
        counts[i] = (playlist_load(names[i], &pl) == PL_OK) ? pl.count
                                                            : SIZE_MAX;
        if (counts[i] != SIZE_MAX)
        {
            playlist_free(&pl);
        }

        size_t len = strlen(names[i]);
        if (len > max_width)
        {
            max_width = len;
        }
    }

    for (size_t i = 0; i < count; i++)
    {
        printf("%-*s", (int)(max_width + 2), names[i]);
        if (counts[i] == SIZE_MAX)
        {
            printf("%s?%s\n", out_sgr(stdout, "1;31"), out_reset(stdout));
        }
        else if (counts[i] == 0)
        {
            printf("%sempty%s\n", out_sgr(stdout, "2"), out_reset(stdout));
        }
        else
        {
            printf("%zu song%s\n", counts[i],
                   (counts[i] == 1) ? "" : "s");
        }
    }

    free(counts);
    playlist_list_free(names, count);
    return 0;
}

/* ------------------------------------------------------------------------ */
/* song subcommands                                                          */
/* ------------------------------------------------------------------------ */

static int cmd_song_add(int argc, char **argv)
{
    if (argc != 3)
    {
        return usage_error("playlist song add", "exactly three arguments",
                           "vyt playlist song add <playlist> <song_name> <url>");
    }

    const char *name = argv[0];
    const char *song_name = argv[1];
    const char *url = argv[2];

    pl_status_t status = playlist_song_add(name, song_name, url);
    if (status != PL_OK)
    {
        return fail_ctx("add a song to", name, song_name, url, status);
    }

    out_ok(stdout, "added '%s' to '%s' (%s)", song_name, name, url);
    return 0;
}

static int cmd_song_remove(int argc, char **argv)
{
    if (argc != 2)
    {
        return usage_error("playlist song remove", "exactly two arguments",
                           "vyt playlist song remove <playlist> <url>");
    }

    const char *name = argv[0];
    const char *url = argv[1];

    pl_status_t status = playlist_song_remove(name, url);
    if (status != PL_OK)
    {
        return fail_ctx("remove a song from", name, NULL, url, status);
    }

    /* The URL is the canonical identifier, so it is what the confirmation
     * names: it is exactly the line that left the file. */
    out_ok(stdout, "removed %s from '%s'", url, name);
    return 0;
}

static int cmd_song_list(int argc, char **argv)
{
    if (argc != 1)
    {
        return usage_error("playlist song list", "exactly one playlist name",
                           "vyt playlist song list <playlist>");
    }

    const char *name = argv[0];

    playlist_t pl;
    pl_status_t status = playlist_load(name, &pl);
    if (status != PL_OK)
    {
        return fail_ctx("list songs in", name, NULL, NULL, status);
    }

    if (pl.count == 0)
    {
        out_status(stderr, "'%s' has no songs yet", name);
        out_hint(stderr,
                 "add one with 'vyt playlist song add %s <song_name> <url>'",
                 name);
        playlist_free(&pl);
        return 0;
    }

    if (!out_stdout_is_tty())
    {
        /* Scriptable mode: the storage format, one song per line.
         * A song without a name is a legacy entry from a previous version
         * of vyt; print the URL alone so the listing stays parseable. */
        for (size_t i = 0; i < pl.count; i++)
        {
            if (pl.songs[i].name == NULL || pl.songs[i].name[0] == '\0')
            {
                printf("%s\n", pl.songs[i].url);
            }
            else
            {
                printf("%s = %s\n", pl.songs[i].name, pl.songs[i].url);
            }
        }
        playlist_free(&pl);
        return 0;
    }

    /* Interactive mode: song names on their own line, URLs dimmed beneath
     * them, so long names and long URLs never fight for the same column. */
    fprintf(stdout, "%s%zu song%s in '%s':%s\n", out_sgr(stdout, "1"),
            pl.count, (pl.count == 1) ? "" : "s", name, out_reset(stdout));

    size_t ncols = num_cols(pl.count);
    int width = out_term_width(stdout);
    size_t indent = ncols + 2; /* "NN. " */

    for (size_t i = 0; i < pl.count; i++)
    {
        if (pl.songs[i].name != NULL && pl.songs[i].name[0] != '\0')
        {
            size_t max_name = 40;
            if (width > (int)(indent + 1) &&
                (size_t)width - indent - 1 < max_name)
            {
                max_name = (size_t)width - indent - 1;
            }
            if (max_name > 256)
            {
                max_name = 256;
            }

            char nbuf[268];
            out_truncate_utf8(nbuf, sizeof nbuf, pl.songs[i].name, max_name);
            fprintf(stdout, "%s%*zu.%s %s%s%s\n", out_sgr(stdout, "2"),
                    (int)ncols, i + 1, out_reset(stdout),
                    out_sgr(stdout, "1"), nbuf, out_reset(stdout));
            fprintf(stdout, "%s%*s%s%s\n", out_sgr(stdout, "2"),
                    (int)(ncols + 2), "", pl.songs[i].url,
                    out_reset(stdout));
        }
        else
        {
            /* Legacy entry: the URL is all we know, so it takes the title
             * slot (dimmed to keep it clearly secondary). */
            fprintf(stdout, "%s%*zu.%s %s%s%s\n", out_sgr(stdout, "2"),
                    (int)ncols, i + 1, out_reset(stdout),
                    out_sgr(stdout, "2"), pl.songs[i].url,
                    out_reset(stdout));
        }
    }

    playlist_free(&pl);
    return 0;
}

static int cmd_song(int argc, char **argv)
{
    if (argc < 1)
    {
        out_error(stderr, "'playlist song' requires a command");
        print_song_usage(stderr);
        return 1;
    }

    const char *command = argv[0];

    if (strcmp(command, "add") == 0)
    {
        return cmd_song_add(argc - 1, argv + 1);
    }

    if (strcmp(command, "remove") == 0)
    {
        return cmd_song_remove(argc - 1, argv + 1);
    }

    if (strcmp(command, "list") == 0)
    {
        return cmd_song_list(argc - 1, argv + 1);
    }

    out_error(stderr, "unknown song command '%s'", command);
    print_song_usage(stderr);
    return 1;
}

/* ========================================================================= */
/* PLAYBACK: `vyt playlist play <name>`                                      */
/* ========================================================================= */

/*
 * The only thing the SIGINT/SIGTERM handler is allowed to do is set a flag.
 * printf, malloc, pthread_mutex_lock, etc. are all undefined behavior inside
 * a signal handler.  volatile sig_atomic_t is the C-standard answer.
 *
 * The main loop checks this flag *between* songs only; once mpv has been
 * exec'd, Ctrl+C kills the whole foreground process group (vyt + mpv) and
 * waitpid() returns.  At that point the loop checks the flag, sees it set,
 * and stops without starting the next song.
 */
static volatile sig_atomic_t g_should_stop = 0;

static void on_stop_signal(int signum)
{
    (void)signum;
    g_should_stop = 1;
}

/* Install our handler for the duration of this command, remembering the
 * previously-installed one so we can restore it on exit.  We use sigaction
 * rather than signal() because signal()'s semantics vary across platforms
 * (BSD vs. SYSV); sigaction is the POSIX-strict answer. */
static int install_stop_handler(struct sigaction *previous_int,
                                struct sigaction *previous_term)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_stop_signal;
    /* Do NOT set SA_RESTART: we want forked mpv's read()/write() to be
     * interrupted by SIGINT so it dies promptly when Ctrl+C is pressed. */
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, previous_int) != 0)
    {
        return -1;
    }
    if (sigaction(SIGTERM, &sa, previous_term) != 0)
    {
        return -1;
    }
    return 0;
}

static void restore_stop_handler(const struct sigaction *previous_int,
                                 const struct sigaction *previous_term)
{
    if (previous_int != NULL)
    {
        sigaction(SIGINT, previous_int, NULL);
    }
    if (previous_term != NULL)
    {
        sigaction(SIGTERM, previous_term, NULL);
    }
}

/*
 * Draw one byte of entropy from the kernel CRNG via getrandom(2).
 *
 * We use getrandom() rather than rand() because:
 *   - rand() has process-global state, which would race with future tests;
 *   - rand() needs an explicit srand() call to be non-deterministic across
 *     runs of the same binary, which is bad UX for a shuffle feature;
 *   - getrandom() reads /dev/urandom and never blocks after early boot.
 *
 * Returns 0 on success, -1 on error (errno preserved for the caller).
 */
static int draw_random_byte(unsigned char *out)
{
    ssize_t got = getrandom(out, 1, 0);
    if (got != 1)
    {
        /* EINTR is possible if a signal arrives mid-syscall, though our
         * handler is only armed during playback and doesn't fire here. */
        if (got == -1 && errno == EINTR)
        {
            return draw_random_byte(out);
        }
        return -1;
    }
    return 0;
}

/*
 * Pick a song index in [0, count) that is NOT `previous`.
 *
 * Strategy:
 *   - If count == 1, the only legal index is 0; we have no choice but to
 *     return it.  This matches the spec's "more than one song" qualifier.
 *   - Otherwise draw a random byte, modulo (count - 1), and shift up if the
 *     result collides with `previous`.  This yields a uniform distribution
 *     over the (count - 1) legal choices, with no rejection loop.
 *
 * `previous` may be SIZE_MAX to mean "no previous song yet", which lets the
 * first pick range over the full [0, count).
 */
static size_t pick_next_index(size_t count, size_t previous)
{
    if (count == 1)
    {
        return 0;
    }

    unsigned char r;
    if (draw_random_byte(&r) != 0)
    {
        /* Fall back to a deterministic-but-non-constant pick so playback
         * doesn't hard-stop on a transient getrandom() failure.  errno is
         * preserved for the caller to print. */
        r = (unsigned char)(previous % 251);
    }

    size_t span = count - 1;
    size_t idx = (size_t)r % span;

    /* The naive modulo gives us [0, span).  We want every legal index
     * except `previous`; if idx >= previous we shift it up by one to skip
     * the gap, yielding [0, previous) U (previous, count) uniformly. */
    if (idx >= previous)
    {
        idx += 1;
    }

    return idx;
}

static int cmd_play(int argc, char **argv)
{
    if (argc != 1)
    {
        return usage_error("playlist play", "exactly one playlist name",
                           "vyt playlist play <playlist>");
    }

    const char *name = argv[0];

    playlist_t pl;
    pl_status_t status = playlist_load(name, &pl);
    if (status != PL_OK)
    {
        return fail_ctx("play", name, NULL, NULL, status);
    }

    if (pl.count == 0)
    {
        /* Not an error: an empty playlist is a valid, if short, listening
         * session.  Tell the user how to populate it and exit cleanly. */
        out_status(stderr, "'%s' has no songs", name);
        out_hint(stderr,
                 "add one with 'vyt playlist song add %s <song_name> <url>'",
                 name);
        playlist_free(&pl);
        return 0;
    }

    /* Validate every URL before playing the first one.  This way the user
     * doesn't sit through three songs only to discover the fourth was
     * malformed.  We use playlist_check_url(), which is exactly the same
     * validator the `song add` path uses, so a hand-edited file gets the
     * same treatment as one built via the CLI.  (In practice playlist_load()
     * already skipped and warned about invalid lines, so this is a
     * belt-and-braces check over the in-memory copy.) */
    for (size_t i = 0; i < pl.count; i++)
    {
        if (playlist_check_url(pl.songs[i].url) != PL_OK)
        {
            out_error(stderr, "%s: entry %zu: '%s' is not a valid URL",
                      name, i + 1, pl.songs[i].url);
            playlist_free(&pl);
            return 1;
        }
    }

    out_status(stdout, "shuffling '%s' (%zu song%s) \u2014 press Ctrl+C to stop",
               name, pl.count, (pl.count == 1) ? "" : "s");
    fflush(stdout);

    /* Install the stop handler.  We save the previous handlers so the
     * terminal's existing SIGINT behavior (e.g. back to a custom shell
     * trap) is restored when we exit. */
    struct sigaction prev_int, prev_term;
    if (install_stop_handler(&prev_int, &prev_term) != 0)
    {
        out_error(stderr, "failed to install signal handler: %s",
                  strerror(errno));
        playlist_free(&pl);
        return 1;
    }

    size_t previous = SIZE_MAX; /* no previous song yet */
    int exit_code = 0;

    while (!g_should_stop)
    {
        size_t idx = pick_next_index(pl.count, previous);
        const char *url = pl.songs[idx].url;
        const char *song_name = pl.songs[idx].name;

        /* Show the song name if there is one; fall back to the URL for
         * legacy entries so the user still sees what is playing.  The
         * bracketed index is the song's position in the playlist. */
        if (song_name != NULL && song_name[0] != '\0')
        {
            out_status(stdout, "now playing [%zu/%zu]: %s%s%s", idx + 1,
                       pl.count, out_sgr(stdout, "1"), song_name,
                       out_reset(stdout));
        }
        else
        {
            out_status(stdout, "now playing [%zu/%zu]: %s%s%s", idx + 1,
                       pl.count, out_sgr(stdout, "2"), url,
                       out_reset(stdout));
        }
        fflush(stdout);

        int rc = player_play_music(url);

        /* mpv exit code 0 == normal end of track.  mpv exit code != 0 could
         * mean a network blip, an age-restricted video, or a dead URL; we
         * print a warning and move on so one bad song doesn't kill the
         * whole session. */
        if (rc != 0 && !g_should_stop)
        {
            out_warn(stderr,
                     "mpv exited with status %d for '%s'; skipping to the next song",
                     rc, url);
        }

        previous = idx;
    }

    restore_stop_handler(&prev_int, &prev_term);

    /* This is the canonical Unix way to acknowledge Ctrl+C: print a newline
     * so the user's prompt doesn't sit on the same line as our ^C glyph,
     * then exit 0 because stopping playback is normal, not an error. */
    printf("\n");
    out_status(stdout, "stopped.");
    playlist_free(&pl);
    return exit_code;
}

/* ========================================================================= */
/* DISPATCH                                                                  */
/* ========================================================================= */

int playlist_cmd_run(int argc, char **argv)
{
    if (argc < 1)
    {
        out_error(stderr, "'playlist' requires a command");
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

    if (strcmp(command, "help") == 0 ||
        strcmp(command, "-h") == 0 ||
        strcmp(command, "--help") == 0)
    {
        playlist_cmd_print_help(stdout);
        return 0;
    }

    if (strcmp(command, "song") == 0)
    {
        return cmd_song(argc - 1, argv + 1);
    }

    if (strcmp(command, "play") == 0)
    {
        return cmd_play(argc - 1, argv + 1);
    }

    out_error(stderr, "unknown playlist command '%s'", command);
    out_hint(stderr, "run 'vyt playlist help' to list the commands");
    return 1;
}
