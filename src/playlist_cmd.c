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
#include <string.h>
#include <sys/random.h>
#include <unistd.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "player.h"
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
            "  vyt playlist song add <playlist> <song_name> <url>\n"
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

/* ------------------------------------------------------------------------ */
/* song subcommands                                                          */
/* ------------------------------------------------------------------------ */

static int cmd_song_add(int argc, char **argv)
{
    if (argc != 3)
    {
        return usage_error("playlist song add",
                           "a playlist name, a song name, and one URL");
    }

    const char *name = argv[0];
    const char *song_name = argv[1];
    const char *url = argv[2];

    pl_status_t status = playlist_song_add(name, song_name, url);
    if (status != PL_OK)
    {
        /* Name the thing that is wrong: the playlist, the song name, or the URL. */
        const char *subject = name;
        if (status == PL_ERR_URL || status == PL_ERR_DUPLICATE)
        {
            subject = url;
        }
        else if (status == PL_ERR_SONG_NAME)
        {
            subject = song_name;
        }
        return report(subject, status);
    }

    printf("added to '%s': %s = %s\n", name, song_name, url);
    return 0;
}

static int cmd_song_remove(int argc, char **argv)
{
    if (argc != 2)
    {
        return usage_error("playlist song remove", "a playlist name and one URL");
    }

    const char *name = argv[0];
    const char *url = argv[1];

    pl_status_t status = playlist_song_remove(name, url);
    if (status != PL_OK)
    {
        const char *subject = (status == PL_ERR_URL || status == PL_ERR_NO_SONG) ? url : name;
        return report(subject, status);
    }

    printf("removed from '%s': %s\n", name, url);
    return 0;
}

static int cmd_song_list(int argc, char **argv)
{
    if (argc != 1)
    {
        return usage_error("playlist song list", "exactly one playlist name");
    }

    const char *name = argv[0];

    playlist_t pl;
    pl_status_t status = playlist_load(name, &pl);
    if (status != PL_OK)
    {
        return report(name, status);
    }

    if (pl.count == 0)
    {
        fprintf(stderr,
                "vyt: '%s' has no songs yet "
                "(add one with 'vyt playlist song add %s <song_name> <url>')\n",
                name, name);
        playlist_free(&pl);
        return 0;
    }

    for (size_t i = 0; i < pl.count; i++)
    {
        /* A song without a name is a legacy entry from a previous version
         * of vyt; print the URL alone so the listing stays parseable and
         * useful.  New entries always have a name. */
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

static int cmd_song(int argc, char **argv)
{
    if (argc < 1)
    {
        fprintf(stderr, "vyt: error: 'playlist song' requires a command\n");
        playlist_cmd_print_usage(stderr);
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

    fprintf(stderr, "vyt: error: unknown song command '%s'\n", command);
    playlist_cmd_print_usage(stderr);
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
        return usage_error("playlist play", "exactly one playlist name");
    }

    const char *name = argv[0];

    playlist_t pl;
    pl_status_t status = playlist_load(name, &pl);
    if (status != PL_OK)
    {
        return report(name, status);
    }

    if (pl.count == 0)
    {
        /* Not an error: an empty playlist is a valid, if short, listening
         * session.  Tell the user how to populate it and exit cleanly. */
        fprintf(stderr,
                "vyt: playlist '%s' is empty "
                "(add songs with 'vyt playlist song add %s <song_name> <url>')\n",
                name, name);
        playlist_free(&pl);
        return 0;
    }

    /* Validate every URL before playing the first one.  This way the user
     * doesn't sit through three songs only to discover line 4 was malformed.
     * We use playlist_check_url(), which is exactly the same validator the
     * `song add` path uses, so a hand-edited file gets the same treatment
     * as one built via the CLI. */
    for (size_t i = 0; i < pl.count; i++)
    {
        if (playlist_check_url(pl.songs[i].url) != PL_OK)
        {
            fprintf(stderr,
                    "vyt: error: %s: line %zu: '%s' is not a valid URL\n",
                    name, i + 1, pl.songs[i].url);
            playlist_free(&pl);
            return 1;
        }
    }

    printf("vyt: shuffling '%s' (%zu song%s). Press Ctrl+C to stop.\n",
           name, pl.count, pl.count == 1 ? "" : "s");
    fflush(stdout);

    /* Install the stop handler.  We save the previous handlers so the
     * terminal's existing SIGINT behavior (e.g. back to a custom shell
     * trap) is restored when we exit. */
    struct sigaction prev_int, prev_term;
    if (install_stop_handler(&prev_int, &prev_term) != 0)
    {
        fprintf(stderr,
                "vyt: error: failed to install signal handler: %s\n",
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
         * legacy entries so the user still sees what is playing. */
        if (song_name != NULL && song_name[0] != '\0')
        {
            printf("vyt: now playing [%zu/%zu]: %s\n",
                   idx + 1, pl.count, song_name);
        }
        else
        {
            printf("vyt: now playing [%zu/%zu]: %s\n",
                   idx + 1, pl.count, url);
        }
        fflush(stdout);

        int rc = player_play_music(url);

        /* mpv exit code 0 == normal end of track.  mpv exit code != 0 could
         * mean a network blip, an age-restricted video, or a dead URL; we
         * print a warning and move on so one bad song doesn't kill the
         * whole session. */
        if (rc != 0 && !g_should_stop)
        {
            fprintf(stderr,
                    "vyt: warning: mpv exited with status %d for '%s'; "
                    "continuing to next song\n",
                    rc, url);
        }

        previous = idx;
    }

    restore_stop_handler(&prev_int, &prev_term);

    /* This is the canonical Unix way to acknowledge Ctrl+C: print a newline
     * so the user's prompt doesn't sit on the same line as our ^C glyph,
     * then exit 0 because stopping playback is normal, not an error. */
    printf("\nvyt: stopped.\n");
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

    if (strcmp(command, "help") == 0 ||
        strcmp(command, "-h") == 0 ||
        strcmp(command, "--help") == 0)
    {
        playlist_cmd_print_usage(stdout);
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

    fprintf(stderr, "vyt: error: unknown playlist command '%s'\n", command);
    playlist_cmd_print_usage(stderr);
    return 1;
}
