#define _POSIX_C_SOURCE 200809L

/* ========================================================================= */
/* SYSTEM & C STANDARD LIBRARIES                                             */
/* ========================================================================= */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ========================================================================= */
/* LOCAL MODULES                                                             */
/* ========================================================================= */
#include "playlist.h"

/* ========================================================================= */
/* STATUS TEXT                                                               */
/* ========================================================================= */

const char *playlist_strerror(pl_status_t status)
{
    switch (status)
    {
    case PL_OK:
        return "success";
    case PL_ERR_NAME:
        return "invalid playlist name";
    case PL_ERR_SONG_NAME:
        return "invalid song name";
    case PL_ERR_URL:
        return "invalid URL";
    case PL_ERR_NOT_FOUND:
        return "no such playlist";
    case PL_ERR_EXISTS:
        return "playlist already exists";
    case PL_ERR_DUPLICATE:
        return "URL is already in the playlist";
    case PL_ERR_NO_SONG:
        return "URL is not in the playlist";
    case PL_ERR_EMPTY:
        return "playlist is empty";
    case PL_ERR_CONFIG:
        return "cannot determine the configuration directory";
    case PL_ERR_MEMORY:
        return "out of memory";
    case PL_ERR_IO:
    default:
        return "filesystem error";
    }
}

/* ========================================================================= */
/* PATHS                                                                     */
/* ========================================================================= */

/* mkdir(path), treating "already there and is a directory" as success. */
static pl_status_t ensure_dir(const char *path)
{
    if (mkdir(path, 0755) == 0)
    {
        return PL_OK;
    }

    if (errno != EEXIST)
    {
        return PL_ERR_IO;
    }

    struct stat st;
    if (stat(path, &st) != 0)
    {
        return PL_ERR_IO;
    }

    if (!S_ISDIR(st.st_mode))
    {
        errno = ENOTDIR;
        return PL_ERR_IO;
    }

    return PL_OK;
}

/* Create every component of an absolute path, like `mkdir -p`.
 * The buffer is written to and restored, so it must be mutable. */
static pl_status_t mkdir_p(char *path)
{
    for (char *p = path + 1; *p != '\0'; p++)
    {
        if (*p != '/')
        {
            continue;
        }

        *p = '\0';
        pl_status_t status = ensure_dir(path);
        *p = '/';

        if (status != PL_OK)
        {
            return status;
        }
    }

    return ensure_dir(path);
}

/* Absolute path of the user's home directory, or NULL. */
static const char *home_dir(void)
{
    const char *home = getenv("HOME");
    if (home != NULL && home[0] == '/')
    {
        return home;
    }

    /* HOME unset (cron, some daemons): fall back to the password database. */
    struct passwd *pw = getpwuid(getuid());
    if (pw != NULL && pw->pw_dir != NULL && pw->pw_dir[0] == '/')
    {
        return pw->pw_dir;
    }

    return NULL;
}

pl_status_t playlist_dir(char *buf, size_t size)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    int n;

    if (xdg != NULL && xdg[0] == '/')
    {
        n = snprintf(buf, size, "%s/vyt/playlists", xdg);
    }
    else
    {
        const char *home = home_dir();
        if (home == NULL)
        {
            return PL_ERR_CONFIG;
        }
        n = snprintf(buf, size, "%s/.config/vyt/playlists", home);
    }

    if (n < 0 || (size_t)n >= size)
    {
        return PL_ERR_CONFIG;
    }

    return mkdir_p(buf);
}

/* A playlist name becomes a filename, so it must not be able to escape the
 * playlists directory or confuse the terminal. */
static pl_status_t check_name(const char *name)
{
    if (name == NULL || name[0] == '\0')
    {
        return PL_ERR_NAME;
    }

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        return PL_ERR_NAME;
    }

    size_t len = strlen(name);
    if (len > PL_NAME_MAX)
    {
        return PL_ERR_NAME;
    }

    /* Leading/trailing blanks make a playlist practically impossible to type
     * again, and a name of only blanks looks like an empty line in the list. */
    if (name[0] == ' ' || name[0] == '\t' ||
        name[len - 1] == ' ' || name[len - 1] == '\t')
    {
        return PL_ERR_NAME;
    }

    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)name[i];
        if (c == '/' || c < 0x20 || c == 0x7f)
        {
            return PL_ERR_NAME;
        }
    }

    return PL_OK;
}

/* Full path of one playlist file, creating the directory if needed. */
static pl_status_t playlist_path(const char *name, char *buf, size_t size)
{
    pl_status_t status = check_name(name);
    if (status != PL_OK)
    {
        return status;
    }

    char dir[PATH_MAX];
    status = playlist_dir(dir, sizeof dir);
    if (status != PL_OK)
    {
        return status;
    }

    int n = snprintf(buf, size, "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= size)
    {
        return PL_ERR_NAME;
    }

    return PL_OK;
}

/* ========================================================================= */
/* PLAYLIST-LEVEL OPERATIONS                                                 */
/* ========================================================================= */

pl_status_t playlist_create(const char *name)
{
    char path[PATH_MAX];
    pl_status_t status = playlist_path(name, path, sizeof path);
    if (status != PL_OK)
    {
        return status;
    }

    /* O_EXCL makes "does it exist?" and "create it" a single atomic step,
     * so two vyt processes cannot both believe they created the playlist. */
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0)
    {
        return (errno == EEXIST) ? PL_ERR_EXISTS : PL_ERR_IO;
    }

    if (close(fd) != 0)
    {
        return PL_ERR_IO;
    }

    return PL_OK;
}

pl_status_t playlist_delete(const char *name)
{
    char path[PATH_MAX];
    pl_status_t status = playlist_path(name, path, sizeof path);
    if (status != PL_OK)
    {
        return status;
    }

    if (unlink(path) != 0)
    {
        return (errno == ENOENT) ? PL_ERR_NOT_FOUND : PL_ERR_IO;
    }

    return PL_OK;
}

static int compare_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

pl_status_t playlist_list(char ***names_out, size_t *count_out)
{
    *names_out = NULL;
    *count_out = 0;

    char dir[PATH_MAX];
    pl_status_t status = playlist_dir(dir, sizeof dir);
    if (status != PL_OK)
    {
        return status;
    }

    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        return PL_ERR_IO;
    }

    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;
    status = PL_OK;

    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        /* Skip ".", ".." and any dotfile a user or editor left behind. */
        if (entry->d_name[0] == '.')
        {
            errno = 0;
            continue;
        }

        char path[PATH_MAX];
        int n = snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof path)
        {
            errno = 0;
            continue;
        }

        /* stat() rather than d_type: d_type is not in POSIX. */
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        {
            errno = 0;
            continue;
        }

        if (count == capacity)
        {
            size_t new_capacity = (capacity == 0) ? 8 : capacity * 2;
            char **grown = realloc(names, new_capacity * sizeof *grown);
            if (grown == NULL)
            {
                status = PL_ERR_MEMORY;
                break;
            }
            names = grown;
            capacity = new_capacity;
        }

        names[count] = strdup(entry->d_name);
        if (names[count] == NULL)
        {
            status = PL_ERR_MEMORY;
            break;
        }
        count++;

        errno = 0;
    }

    /* readdir() returning NULL is either end-of-directory or a real error;
     * errno is the only way to tell them apart. */
    if (status == PL_OK && entry == NULL && errno != 0)
    {
        status = PL_ERR_IO;
    }

    int saved_errno = errno;
    closedir(dp);
    errno = saved_errno;

    if (status != PL_OK)
    {
        playlist_list_free(names, count);
        return status;
    }

    if (count > 1)
    {
        qsort(names, count, sizeof *names, compare_names);
    }

    *names_out = names;
    *count_out = count;
    return PL_OK;
}

void playlist_list_free(char **names, size_t count)
{
    if (names == NULL)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        free(names[i]);
    }
    free(names);
}

/* ========================================================================= */
/* VALIDATORS                                                                 */
/* ========================================================================= */

pl_status_t playlist_check_url(const char *url)
{
    if (url == NULL || url[0] == '\0')
    {
        return PL_ERR_URL;
    }

    size_t len = strlen(url);
    if (len > PL_URL_MAX)
    {
        return PL_ERR_URL;
    }

    /* Requiring a scheme does three jobs at once: it rejects nonsense, it
     * keeps one URL on one line, and it guarantees the string can never be
     * mistaken for an mpv option when we exec it (an option starts with -). */
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
    {
        return PL_ERR_URL;
    }

    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)url[i];
        if (c <= 0x20 || c == 0x7f)
        {
            return PL_ERR_URL;
        }
    }

    return PL_OK;
}

pl_status_t playlist_check_song_name(const char *name)
{
    if (name == NULL || name[0] == '\0')
    {
        return PL_ERR_SONG_NAME;
    }

    size_t len = strlen(name);
    if (len > PL_SONG_NAME_MAX)
    {
        return PL_ERR_SONG_NAME;
    }

    /* A name made only of whitespace would look like a blank line once
     * written to disk, so reject it here. */
    int has_nonblank = 0;
    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)name[i];
        /* Newlines would break the one-song-per-line storage format, and
         * other control characters are not something a song title should
         * contain. */
        if (c < 0x20 || c == 0x7f)
        {
            return PL_ERR_SONG_NAME;
        }
        if (c != ' ' && c != '\t')
        {
            has_nonblank = 1;
        }
    }

    if (!has_nonblank)
    {
        return PL_ERR_SONG_NAME;
    }

    return PL_OK;
}

/* ========================================================================= */
/* SONG-LEVEL OPERATIONS                                                     */
/* ========================================================================= */

/* Append one {name,url} pair to the in-memory playlist.
 * Both strings are taken as already-owned; on failure the caller still owns
 * them and must free them. */
static pl_status_t songs_push(playlist_t *pl, char *name, char *url)
{
    if (pl->count == pl->capacity)
    {
        size_t new_capacity = (pl->capacity == 0) ? 8 : pl->capacity * 2;
        song_t *grown = realloc(pl->songs, new_capacity * sizeof *grown);
        if (grown == NULL)
        {
            return PL_ERR_MEMORY;
        }
        pl->songs = grown;
        pl->capacity = new_capacity;
    }

    pl->songs[pl->count].name = name;
    pl->songs[pl->count].url = url;
    pl->count++;
    return PL_OK;
}

/* Strip the newline and any surrounding blanks, in place. */
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }

    size_t len = strlen(s);
    while (len > 0)
    {
        char c = s[len - 1];
        if (c != '\n' && c != '\r' && c != ' ' && c != '\t')
        {
            break;
        }
        s[--len] = '\0';
    }

    return s;
}

/*
 * Split a stored line into {name, url}.
 *
 * Storage format is "<name> = <url>\n".  Because playlist_check_url() rejects
 * any byte <= 0x20 inside a URL, the URL portion can never contain a space,
 * so the LAST " = " in a line is unambiguously the separator -- a song name
 * that itself contains " = " still splits correctly, because everything to
 * the right of the final " = " must be the URL.
 *
 * For backward compatibility with playlists written by previous versions of
 * vyt (URL-per-line, no name), a line without any " = " is treated as a URL
 * with an empty name.
 *
 * Returns 1 if a separator was found (new format), 0 if not (legacy URL-only
 * format).  On return, *name_out and *url_out point into the buffer (which
 * has been mutated in place) and are NUL-terminated.
 */
static int split_song_line(char *line, char **name_out, char **url_out)
{
    /* Walk from the end so a name containing " = " splits at the LAST
     * occurrence, which is the only one that can be the real boundary. */
    size_t len = strlen(line);
    char *sep = NULL;
    for (size_t i = len; i >= 3; i--)
    {
        if (line[i - 1] == ' ' && line[i - 2] == '=' && line[i - 3] == ' ')
        {
            sep = &line[i - 3];
            break;
        }
    }

    if (sep != NULL)
    {
        *sep = '\0';                  /* truncate name side */
        *name_out = line;             /* name is the left part (already trimmed) */
        *url_out = sep + 3;           /* URL is whatever follows " = " */

        /* The URL side may still have trailing whitespace from the line; the
         * caller has already trimmed the line, so this is normally a no-op,
         * but be defensive in case the file was hand-edited. */
        size_t url_len = strlen(*url_out);
        while (url_len > 0 && ((*url_out)[url_len - 1] == ' ' ||
                               (*url_out)[url_len - 1] == '\t'))
        {
            (*url_out)[--url_len] = '\0';
        }
        return 1;
    }

    /* Legacy URL-only line. */
    *name_out = (char *)"";
    *url_out = line;
    return 0;
}

pl_status_t playlist_load(const char *name, playlist_t *out)
{
    out->songs = NULL;
    out->count = 0;
    out->capacity = 0;

    char path[PATH_MAX];
    pl_status_t status = playlist_path(name, path, sizeof path);
    if (status != PL_OK)
    {
        return status;
    }

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        return (errno == ENOENT) ? PL_ERR_NOT_FOUND : PL_ERR_IO;
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;

    while ((n = getline(&line, &cap, fp)) != -1)
    {
        char *trimmed = trim(line);
        if (trimmed[0] == '\0')
        {
            continue; /* blank line left by a hand edit */
        }

        char *song_name_buf = NULL;
        char *url_buf = NULL;
        (void)split_song_line(trimmed, &song_name_buf, &url_buf);

        /* The URL is the canonical identifier; reject the line if it is not
         * a valid URL.  This is the same check the song-add path makes, so
         * a hand-edited file gets the same treatment as one built via CLI. */
        if (playlist_check_url(url_buf) != PL_OK)
        {
            /* Skip malformed line rather than poison the whole playlist; the
             * playback path will re-check and report the exact line. */
            continue;
        }

        char *url_copy = strdup(url_buf);
        if (url_copy == NULL)
        {
            status = PL_ERR_MEMORY;
            break;
        }

        /* song_name_buf may be a pointer into the line buffer (the new-format
         * case) or a pointer to a static "" (the legacy case); either way we
         * need an owned copy so playlist_free can free() it uniformly. */
        char *name_copy = strdup(song_name_buf);
        if (name_copy == NULL)
        {
            free(url_copy);
            status = PL_ERR_MEMORY;
            break;
        }

        status = songs_push(out, name_copy, url_copy);
        if (status != PL_OK)
        {
            free(name_copy);
            free(url_copy);
            break;
        }
    }

    if (status == PL_OK && ferror(fp))
    {
        status = PL_ERR_IO;
    }

    free(line);

    int saved_errno = errno;
    fclose(fp);
    errno = saved_errno;

    if (status != PL_OK)
    {
        playlist_free(out);
    }

    return status;
}

void playlist_free(playlist_t *pl)
{
    if (pl == NULL || pl->songs == NULL)
    {
        return;
    }

    for (size_t i = 0; i < pl->count; i++)
    {
        free(pl->songs[i].name);
        free(pl->songs[i].url);
    }
    free(pl->songs);

    pl->songs = NULL;
    pl->count = 0;
    pl->capacity = 0;
}

/* Replace a playlist file with the given songs, atomically.
 * We write a temporary file and rename() it over the original: rename is
 * atomic, so an interrupted vyt can never leave a half-written playlist. */
static pl_status_t playlist_store(const char *name, const playlist_t *pl)
{
    char dir[PATH_MAX];
    pl_status_t status = playlist_dir(dir, sizeof dir);
    if (status != PL_OK)
    {
        return status;
    }

    char path[PATH_MAX];
    char tmp[PATH_MAX];
    int a = snprintf(path, sizeof path, "%s/%s", dir, name);
    /* The temp name starts with a dot so a crashed run leaves something that
     * playlist_list() skips instead of a playlist named "chill.tmp". */
    int b = snprintf(tmp, sizeof tmp, "%s/.%s.XXXXXX", dir, name);
    if (a < 0 || (size_t)a >= sizeof path || b < 0 || (size_t)b >= sizeof tmp)
    {
        return PL_ERR_NAME;
    }

    int fd = mkstemp(tmp);
    if (fd < 0)
    {
        return PL_ERR_IO;
    }

    /* mkstemp creates the file 0600; playlists are ordinary config files. */
    if (fchmod(fd, 0644) != 0)
    {
        close(fd);
        unlink(tmp);
        return PL_ERR_IO;
    }

    FILE *fp = fdopen(fd, "w");
    if (fp == NULL)
    {
        close(fd);
        unlink(tmp);
        return PL_ERR_IO;
    }

    for (size_t i = 0; i < pl->count; i++)
    {
        const char *song_name = pl->songs[i].name;
        const char *url = pl->songs[i].url;

        /* Songs without a name (legacy entries loaded from an old playlist
         * that we are now rewriting) are stored as URL-only lines so the
         * file round-trips back to the same in-memory state. */
        if (song_name == NULL || song_name[0] == '\0')
        {
            if (fprintf(fp, "%s\n", url) < 0)
            {
                fclose(fp);
                unlink(tmp);
                return PL_ERR_IO;
            }
        }
        else
        {
            if (fprintf(fp, "%s = %s\n", song_name, url) < 0)
            {
                fclose(fp);
                unlink(tmp);
                return PL_ERR_IO;
            }
        }
    }

    /* Flush through the C library, then through the kernel, before renaming:
     * otherwise a power cut can leave the new name pointing at empty data. */
    if (fflush(fp) != 0 || fsync(fileno(fp)) != 0)
    {
        fclose(fp);
        unlink(tmp);
        return PL_ERR_IO;
    }

    if (fclose(fp) != 0)
    {
        unlink(tmp);
        return PL_ERR_IO;
    }

    if (rename(tmp, path) != 0)
    {
        unlink(tmp);
        return PL_ERR_IO;
    }

    return PL_OK;
}

pl_status_t playlist_song_add(const char *name, const char *song_name,
                              const char *url)
{
    pl_status_t status = playlist_check_song_name(song_name);
    if (status != PL_OK)
    {
        return status;
    }

    status = playlist_check_url(url);
    if (status != PL_OK)
    {
        return status;
    }

    /* Loading first also answers "does this playlist exist?". */
    playlist_t pl;
    status = playlist_load(name, &pl);
    if (status != PL_OK)
    {
        return status;
    }

    /* URLs are the canonical identifier; reject duplicates by URL, not by
     * name, so the same track can't appear twice even under two names. */
    for (size_t i = 0; i < pl.count; i++)
    {
        if (strcmp(pl.songs[i].url, url) == 0)
        {
            playlist_free(&pl);
            return PL_ERR_DUPLICATE;
        }
    }

    char *name_copy = strdup(song_name);
    if (name_copy == NULL)
    {
        playlist_free(&pl);
        return PL_ERR_MEMORY;
    }

    char *url_copy = strdup(url);
    if (url_copy == NULL)
    {
        free(name_copy);
        playlist_free(&pl);
        return PL_ERR_MEMORY;
    }

    status = songs_push(&pl, name_copy, url_copy);
    if (status != PL_OK)
    {
        free(name_copy);
        free(url_copy);
        playlist_free(&pl);
        return status;
    }

    status = playlist_store(name, &pl);
    playlist_free(&pl);
    return status;
}

pl_status_t playlist_song_remove(const char *name, const char *url)
{
    if (url == NULL || url[0] == '\0')
    {
        return PL_ERR_URL;
    }

    playlist_t pl;
    pl_status_t status = playlist_load(name, &pl);
    if (status != PL_OK)
    {
        return status;
    }

    size_t index = pl.count;
    for (size_t i = 0; i < pl.count; i++)
    {
        if (strcmp(pl.songs[i].url, url) == 0)
        {
            index = i;
            break;
        }
    }

    if (index == pl.count)
    {
        playlist_free(&pl);
        return PL_ERR_NO_SONG;
    }

    free(pl.songs[index].name);
    free(pl.songs[index].url);
    for (size_t i = index + 1; i < pl.count; i++)
    {
        pl.songs[i - 1] = pl.songs[i];
    }
    pl.count--;

    status = playlist_store(name, &pl);
    playlist_free(&pl);
    return status;
}
