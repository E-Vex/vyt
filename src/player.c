#include <stddef.h>

#include "player.h"
#include "proc.h"

int player_play_music(const char *url)
{
    char *argv[] = {
        "mpv",
        "--no-video",
        (char *)url,
        NULL};
    return proc_run("mpv", argv);
}

int player_play_video(const char *url)
{
    char *argv[] = {
        "mpv",
        (char *)url,
        NULL
    };
    return proc_run("mpv", argv);
}
