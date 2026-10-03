#include <stddef.h>

#include "player.h"
#include "proc.h"

int player_play_music(const char *url)
{
    /* "--" ends mpv's option parsing, so a URL or search term can never be
     * mistaken for an option.  Without it, an argument like "--no-video"
     * (from the CLI or a hand-edited playlist) would be interpreted by mpv
     * as a flag rather than played as media. */
    char *argv[] = {
        "mpv",
        "--no-video",
        "--",
        (char *)url,
        NULL};
    return proc_run("mpv", argv);
}

int player_play_video(const char *url)
{
    char *argv[] = {
        "mpv",
        "--",
        (char *)url,
        NULL};
    return proc_run("mpv", argv);
}
