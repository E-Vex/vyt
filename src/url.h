#ifndef URL_H
#define URL_H

/*
 * Turning what the user typed into a URL vyt can hand to mpv or yt-dlp.
 */

/* Why a media reference could not be turned into a URL. */
typedef enum
{
    URL_REF_OK = 0,  /* the reference was understood */
    URL_REF_EMPTY,   /* the reference was empty or only whitespace */
    URL_REF_INVALID  /* the reference is not a YouTube URL or video ID */
} url_ref_status_t;

/*
 * Turn a user-supplied media reference into an absolute http(s) URL.
 *
 * Accepted forms:
 *   https://youtu.be/dQw4w9WgXcQ   a full URL, used as it is
 *   youtu.be/dQw4w9WgXcQ           a YouTube URL without the scheme
 *   www.youtube.com/watch?v=...    likewise
 *   dQw4w9WgXcQ                   a bare video ID
 *
 * Surrounding whitespace is ignored, so a URL pasted with a stray newline
 * still works.
 *
 * Returns a newly allocated URL the caller must free, or NULL when the
 * reference is empty (URL_REF_EMPTY) or unusable (URL_REF_INVALID).
 */
char *url_normalize_ref(const char *input, url_ref_status_t *status);

#endif /* URL_H */
