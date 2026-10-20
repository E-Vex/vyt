#include "download.h"
#include "proc.h"

int download_youtube(const char *url)
{
    /* "--" ends yt-dlp's option parsing, so a URL can never be mistaken
     * for an option (e.g. a URL beginning with "-"). */
    char *argv[] = {
        "yt-dlp",
        "--",
        (char *)url,
        NULL};
    return proc_run("yt-dlp", argv);
}
