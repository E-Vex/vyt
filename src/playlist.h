#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <stddef.h>

/* Longest playlist name we accept, in bytes. */
#define PL_NAME_MAX 64

/* Longest URL we accept, in bytes. */
#define PL_URL_MAX 2048

/* Result of a playlist operation.
 * PL_ERR_IO means a libc call failed and errno is set;
 * every other value is self-describing (see playlist_strerror). */
typedef enum
{
    PL_OK = 0,
    PL_ERR_NAME,      /* invalid playlist name */
    PL_ERR_URL,       /* empty or malformed URL */
    PL_ERR_NOT_FOUND, /* no such playlist */
    PL_ERR_EXISTS,    /* playlist already exists */
    PL_ERR_DUPLICATE, /* URL already in this playlist */
    PL_ERR_NO_SONG,   /* URL not in this playlist */
    PL_ERR_EMPTY,     /* playlist has no songs */
    PL_ERR_CONFIG,    /* cannot determine the config directory */
    PL_ERR_MEMORY,    /* out of memory */
    PL_ERR_IO         /* filesystem error, errno is set */
} pl_status_t;

/* A playlist loaded into memory. */
typedef struct
{
    char **songs; /* array of owned strings, one URL each */
    size_t count;
    size_t capacity;
} playlist_t;

/* Human-readable text for a status. Never returns NULL.
 * For PL_ERR_IO the caller should print strerror(errno) instead. */
const char *playlist_strerror(pl_status_t status);

/* Write the playlists directory path into buf, creating it if needed. */
pl_status_t playlist_dir(char *buf, size_t size);

/* Playlist-level operations. */
pl_status_t playlist_create(const char *name);
pl_status_t playlist_delete(const char *name);
pl_status_t playlist_list(char ***names_out, size_t *count_out);
void playlist_list_free(char **names, size_t count);

/* Is this string acceptable as a playlist entry?
 * Exposed so playback can re-check lines that were hand-edited into the file. */
pl_status_t playlist_check_url(const char *url);

/* Song-level operations. */
pl_status_t playlist_load(const char *name, playlist_t *out);
void playlist_free(playlist_t *pl);
pl_status_t playlist_song_add(const char *name, const char *url);
pl_status_t playlist_song_remove(const char *name, const char *url);

#endif /* PLAYLIST_H */
