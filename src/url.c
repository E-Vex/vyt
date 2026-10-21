#define _POSIX_C_SOURCE 200809L

/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "url.h"

/* Hosts that mean YouTube when the scheme is missing. */
static const char *const youtube_hosts[] = {
    "youtu.be/",
    "www.youtu.be/",
    "youtube.com/",
    "www.youtube.com/",
    "m.youtube.com/",
    "music.youtube.com/",
};

/* Length of a YouTube video ID. */
#define VIDEO_ID_LEN 11

/* A video ID is exactly 11 characters from the base64url alphabet. */
static int is_video_id(const char *s, size_t len)
{
    if (len != VIDEO_ID_LEN)
    {
        return 0;
    }

    for (size_t i = 0; i < len; i++)
    {
        char c = s[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok)
        {
            return 0;
        }
    }
    return 1;
}

/* Does `s` start with `prefix`, ignoring case? */
static int starts_with_ci(const char *s, const char *prefix)
{
    return strncasecmp(s, prefix, strlen(prefix)) == 0;
}

/* Allocate `text` prefixed by `prefix`. */
static char *join(const char *prefix, const char *text, size_t len)
{
    size_t prefix_len = strlen(prefix);
    char *out = malloc(prefix_len + len + 1);
    if (out == NULL)
    {
        return NULL;
    }
    memcpy(out, prefix, prefix_len);
    memcpy(out + prefix_len, text, len);
    out[prefix_len + len] = '\0';
    return out;
}

char *url_normalize_ref(const char *input, url_ref_status_t *status)
{
    if (status != NULL)
    {
        *status = URL_REF_INVALID;
    }
    if (input == NULL)
    {
        if (status != NULL)
        {
            *status = URL_REF_EMPTY;
        }
        return NULL;
    }

    /* Ignore surrounding whitespace: URLs are often pasted along with a
     * space or a trailing newline. */
    const char *start = input;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')
    {
        start++;
    }

    size_t len = strlen(start);
    while (len > 0)
    {
        char c = start[len - 1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
        {
            break;
        }
        len--;
    }

    if (len == 0)
    {
        if (status != NULL)
        {
            *status = URL_REF_EMPTY;
        }
        return NULL;
    }

    /* A full URL is used as it is. */
    if (starts_with_ci(start, "http://") || starts_with_ci(start, "https://"))
    {
        char *copy = malloc(len + 1);
        if (copy == NULL)
        {
            return NULL;
        }
        memcpy(copy, start, len);
        copy[len] = '\0';
        if (status != NULL)
        {
            *status = URL_REF_OK;
        }
        return copy;
    }

    /* A YouTube URL without its scheme gets one. */
    for (size_t i = 0; i < sizeof youtube_hosts / sizeof youtube_hosts[0]; i++)
    {
        size_t host_len = strlen(youtube_hosts[i]);
        if (len > host_len && strncasecmp(start, youtube_hosts[i], host_len) == 0)
        {
            char *url = join("https://", start, len);
            if (url != NULL && status != NULL)
            {
                *status = URL_REF_OK;
            }
            return url;
        }
    }

    /* A bare video ID becomes a youtu.be link. */
    if (is_video_id(start, len))
    {
        char *url = join("https://youtu.be/", start, len);
        if (url != NULL && status != NULL)
        {
            *status = URL_REF_OK;
        }
        return url;
    }

    return NULL;
}
