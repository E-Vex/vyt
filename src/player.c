#include <stddef.h>

#include "player.h"
#include "proc.h"

int player_play_music(const char *url)
{
    /* "--" ends mpv's option parsing, so a URL or search term can never be
     * mistaken for an option.  Without it, an argument like "--no-video"
     * (from the CLI or a hand-edited playlist) would be interpreted by mpv
     * as a flag rather than played as media. */
    char *url_mutable = (char *)url;  /* execvp does not modify argv strings */
    char *const argv[] = {
        "mpv",
        "--no-video",
        "--",
        url_mutable,
        NULL};
    return proc_run("mpv", argv);
}

int player_play_video(const char *url)
{
    char *url_mutable = (char *)url;  /* execvp does not modify argv strings */
    char *const argv[] = {
        "mpv",
        "--",
        url_mutable,
        NULL};
    return proc_run("mpv", argv);
}
