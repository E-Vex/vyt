#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "search.h"
#include "proc.h"

#define SEARCH_PREFIX "ytsearch10:"

int search_youtube(const char *query) {
    size_t len = strlen(SEARCH_PREFIX) + strlen(query) + 1;
    char *search_term = malloc(len);
    if (search_term == NULL) {
        fprintf(stderr, "vyt: error: out of memory\n");
        return -1;
    }
    snprintf(search_term, len, "%s%s", SEARCH_PREFIX, query);

    char *argv[] = {
        "yt-dlp",
        "--flat-playlist",
        "--print", "%(title)s | %(webpage_url)s",
        search_term,
        NULL
    };

    int status = proc_run("yt-dlp", argv);

    free(search_term);
    return status;
}
