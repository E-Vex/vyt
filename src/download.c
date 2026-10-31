#include "download.h"
#include "proc.h"

int download_youtube(const char *url)
{
    /* "--" ends yt-dlp's option parsing, so a URL can never be mistaken
     * for an option (e.g. a URL beginning with "-"). */
    char *url_mutable = (char *)url;  /* execvp does not modify argv strings */
    char *const argv[] = {
        "yt-dlp",
        "--",
        url_mutable,
        NULL};
    return proc_run("yt-dlp", argv);
}
