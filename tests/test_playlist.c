#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/playlist.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg)                                                    \
    do                                                                      \
    {                                                                       \
        checks++;                                                           \
        if (!(cond))                                                        \
        {                                                                   \
            failures++;                                                     \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        }                                                                   \
    } while (0)

#define CHECK_STREQ(got, want, msg)                                         \
    do                                                                      \
    {                                                                       \
        const char *g_ = (got);                                             \
        const char *w_ = (want);                                            \
        checks++;                                                           \
        if (g_ == NULL || strcmp(g_, w_) != 0)                              \
        {                                                                   \
            failures++;                                                     \
            fprintf(stderr, "FAIL: %s (got \"%s\", want \"%s\") (%s:%d)\n", \
                    msg, g_ ? g_ : "(null)", w_, __FILE__, __LINE__);       \
        }                                                                   \
    } while (0)

/* ------------------------------------------------------------------------ */
/* helpers                                                                   */
/* ------------------------------------------------------------------------ */

/* Recursively delete tmp and everything inside it. */
static void rmtree(const char *tmp)
{
    DIR *dp = opendir(tmp);
    if (dp != NULL)
    {
        struct dirent *entry;
        while ((entry = readdir(dp)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            char path[4096];
            snprintf(path, sizeof path, "%s/%s", tmp, entry->d_name);

            struct stat st;
            if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode))
            {
                rmtree(path);
            }
            else
            {
                unlink(path);
            }
        }
        closedir(dp);
    }
    rmdir(tmp);
}

static void test_dir_is_created(void)
{
    char buf[4096];
    pl_status_t st = playlist_dir(buf, sizeof buf);

    CHECK(st == PL_OK, "dir: playlist_dir succeeds");
    CHECK(strstr(buf, "/vyt/playlists") != NULL, "dir: path ends with vyt/playlists");

    struct stat sb;
    CHECK(stat(buf, &sb) == 0 && S_ISDIR(sb.st_mode), "dir: directory exists on disk");
}

static void test_name_validation(void)
{
    /* Invalid names must be rejected without touching the filesystem;
     * playlist_load is the cheapest path that runs check_name(). */
    const char *bad[] = {
        "",                       /* empty            */
        ".",                      /* this directory   */
        "..",                     /* parent directory */
        ".hidden",                /* invisible in list: dotfiles are skipped */
        "a/b",                    /* path separator   */
        "trailing ",              /* trailing blank   */
        " leading",               /* leading blank    */
        "new\nline",              /* control char     */
        "tab\tname",              /* control char     */
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" /* 65 bytes */
    };

    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        playlist_t pl;
        CHECK(playlist_load(bad[i], &pl) == PL_ERR_NAME, "name: rejects invalid name");
    }

    /* 64 bytes is the documented maximum and must be accepted. */
    const char *ok64 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    CHECK(strlen(ok64) == 64, "name: boundary string is 64 bytes");
    CHECK(playlist_load(ok64, &(playlist_t){0}) == PL_ERR_NOT_FOUND,
          "name: 64-byte name is valid (playlist merely missing)");
}

static void test_create_duplicate_delete(void)
{
    CHECK(playlist_create("t-create") == PL_OK, "create: new playlist succeeds");
    CHECK(playlist_create("t-create") == PL_ERR_EXISTS, "create: duplicate rejected");
    CHECK(playlist_delete("t-create") == PL_OK, "delete: existing playlist removed");
    CHECK(playlist_delete("t-create") == PL_ERR_NOT_FOUND, "delete: missing playlist reported");
    CHECK(playlist_create("") == PL_ERR_NAME, "create: empty name rejected");
}

static void test_list_sorted_and_filtered(void)
{
    CHECK(playlist_create("t-list-b") == PL_OK, "list: create b");
    CHECK(playlist_create("t-list-a") == PL_OK, "list: create a");
    CHECK(playlist_create("t-list-c") == PL_OK, "list: create c");

    /* A dotfile (e.g. a crashed temp file) and a directory must be skipped. */
    char dir[4096];
    CHECK(playlist_dir(dir, sizeof dir) == PL_OK, "list: dir resolves");

    char path[4200];
    snprintf(path, sizeof path, "%s/.t-list-crash.tmp", dir);
    FILE *fp = fopen(path, "w");
    CHECK(fp != NULL, "list: dotfile created");
    if (fp != NULL)
    {
        fputs("junk = https://example.com/junk\n", fp);
        fclose(fp);
    }

    snprintf(path, sizeof path, "%s/t-list-dir", dir);
    CHECK(mkdir(path, 0755) == 0, "list: subdirectory created");

    char **names = NULL;
    size_t count = 0;
    pl_status_t st = playlist_list(&names, &count);
    CHECK(st == PL_OK, "list: succeeds");
    CHECK(count == 3, "list: dotfile and subdirectory skipped");
    if (count == 3)
    {
        CHECK_STREQ(names[0], "t-list-a", "list: sorted a");
        CHECK_STREQ(names[1], "t-list-b", "list: sorted b");
        CHECK_STREQ(names[2], "t-list-c", "list: sorted c");
    }
    playlist_list_free(names, count);

    CHECK(playlist_delete("t-list-a") == PL_OK, "list: cleanup a");
    CHECK(playlist_delete("t-list-b") == PL_OK, "list: cleanup b");
    CHECK(playlist_delete("t-list-c") == PL_OK, "list: cleanup c");
}

static void test_song_add_and_list(void)
{
    CHECK(playlist_create("t-songs") == PL_OK, "song add: playlist created");
    CHECK(playlist_song_add("t-songs", "Nightcall", "https://youtu.be/AAAA") == PL_OK,
          "song add: first song succeeds");
    CHECK(playlist_song_add("t-songs", "Teardrop", "https://youtu.be/BBBB") == PL_OK,
          "song add: second song succeeds");

    /* Same URL under a different name is still a duplicate: URL is canonical. */
    CHECK(playlist_song_add("t-songs", "Other", "https://youtu.be/AAAA") == PL_ERR_DUPLICATE,
          "song add: duplicate URL rejected");
    CHECK(playlist_song_add("t-songs", "Kalkbrenner", "https://youtu.be/AAAA") == PL_ERR_DUPLICATE,
          "song add: duplicate URL rejected regardless of name");

    playlist_t pl;
    CHECK(playlist_load("t-songs", &pl) == PL_OK, "song add: load succeeds");
    CHECK(pl.count == 2, "song add: two songs stored");
    if (pl.count == 2)
    {
        CHECK_STREQ(pl.songs[0].name, "Nightcall", "song add: first name");
        CHECK_STREQ(pl.songs[0].url, "https://youtu.be/AAAA", "song add: first url");
        CHECK_STREQ(pl.songs[1].name, "Teardrop", "song add: second name");
        CHECK_STREQ(pl.songs[1].url, "https://youtu.be/BBBB", "song add: second url");
    }
    playlist_free(&pl);
}

static void test_song_add_validation(void)
{
    CHECK(playlist_song_add("t-missing", "x", "https://youtu.be/AAA") == PL_ERR_NOT_FOUND,
          "song add: missing playlist reported");

    const char *bad_urls[] = {
        "",                 /* empty          */
        "notaurl",          /* no scheme      */
        "ftp://example.com",/* wrong scheme   */
        "http://with space",/* whitespace     */
        "--flag",           /* option-looking */
    };
    for (size_t i = 0; i < sizeof bad_urls / sizeof bad_urls[0]; i++)
    {
        CHECK(playlist_song_add("t-songs", "x", bad_urls[i]) == PL_ERR_URL,
              "song add: invalid URL rejected");
    }

    const char *bad_names[] = {
        "",        /* empty               */
        "   ",     /* blanks only         */
        "a\nb",    /* control character   */
        "a\rb",    /* control character   */
    };
    for (size_t i = 0; i < sizeof bad_names / sizeof bad_names[0]; i++)
    {
        CHECK(playlist_song_add("t-songs", bad_names[i], "https://youtu.be/CCC") == PL_ERR_SONG_NAME,
              "song add: invalid song name rejected");
    }

    /* Length limits. */
    char url[PL_URL_MAX + 2];
    strcpy(url, "https://");
    for (size_t i = 8; i < PL_URL_MAX + 1; i++)
    {
        url[i] = 'a';
    }
    url[PL_URL_MAX + 1] = '\0';
    CHECK(playlist_check_url(url) == PL_ERR_URL, "limits: over-long URL rejected");
    url[PL_URL_MAX] = '\0'; /* now exactly PL_URL_MAX bytes */
    CHECK(playlist_check_url(url) == PL_OK, "limits: max-length URL accepted");

    char long_name[PL_SONG_NAME_MAX + 2];
    memset(long_name, 'a', PL_SONG_NAME_MAX + 1);
    long_name[PL_SONG_NAME_MAX + 1] = '\0';
    CHECK(playlist_check_song_name(long_name) == PL_ERR_SONG_NAME,
          "limits: over-long song name rejected");
}

static void test_song_remove(void)
{
    CHECK(playlist_create("t-remove") == PL_OK, "remove: playlist created");
    CHECK(playlist_song_add("t-remove", "A", "https://youtu.be/RA") == PL_OK, "remove: add A");
    CHECK(playlist_song_add("t-remove", "B", "https://youtu.be/RB") == PL_OK, "remove: add B");

    CHECK(playlist_song_remove("t-remove", "https://youtu.be/RA") == PL_OK, "remove: first removal");
    CHECK(playlist_song_remove("t-remove", "https://youtu.be/RA") == PL_ERR_NO_SONG,
          "remove: second removal reports missing song");
    CHECK(playlist_song_remove("t-remove", "") == PL_ERR_URL, "remove: empty URL rejected");
    CHECK(playlist_song_remove("t-missing", "https://youtu.be/RA") == PL_ERR_NOT_FOUND,
          "remove: missing playlist reported");

    playlist_t pl;
    CHECK(playlist_load("t-remove", &pl) == PL_OK, "remove: reload");
    CHECK(pl.count == 1, "remove: one song left");
    if (pl.count == 1)
    {
        CHECK_STREQ(pl.songs[0].name, "B", "remove: survivor is B");
    }
    playlist_free(&pl);
}

static void test_load_formats(void)
{
    /* Hand-write a playlist mixing all supported line shapes. */
    char dir[4096];
    CHECK(playlist_dir(dir, sizeof dir) == PL_OK, "formats: dir resolves");

    char path[4200];
    snprintf(path, sizeof path, "%s/t-formats", dir);
    FILE *fp = fopen(path, "w");
    CHECK(fp != NULL, "formats: file opened");
    if (fp == NULL)
    {
        return;
    }
    fputs("Nightcall = https://example.com/a\n", fp);
    fputs("https://example.com/b\n", fp);          /* legacy URL-only line  */
    fputs("\n", fp);                                /* blank line            */
    fputs("   \n", fp);                             /* whitespace-only line  */
    fputs("Weird = Name = https://example.com/c\n", fp); /* name with " = "  */
    fputs("https://example.com/d\n", fp);           /* legacy again          */
    fclose(fp);

    playlist_t pl;
    CHECK(playlist_load("t-formats", &pl) == PL_OK, "formats: load succeeds");
    CHECK(pl.count == 4, "formats: blank and whitespace lines ignored");
    if (pl.count == 4)
    {
        CHECK_STREQ(pl.songs[0].name, "Nightcall", "formats: named entry");
        CHECK_STREQ(pl.songs[0].url, "https://example.com/a", "formats: named URL");

        CHECK_STREQ(pl.songs[1].name, "", "formats: legacy entry has empty name");
        CHECK_STREQ(pl.songs[1].url, "https://example.com/b", "formats: legacy URL");

        CHECK_STREQ(pl.songs[2].name, "Weird = Name", "formats: splits at LAST ' = '");
        CHECK_STREQ(pl.songs[2].url, "https://example.com/c", "formats: URL after last separator");

        CHECK_STREQ(pl.songs[3].url, "https://example.com/d", "formats: second legacy entry");
    }
    playlist_free(&pl);
}

static void test_load_skips_malformed_lines(void)
{
    char dir[4096];
    CHECK(playlist_dir(dir, sizeof dir) == PL_OK, "malformed: dir resolves");

    char path[4200];
    snprintf(path, sizeof path, "%s/t-malformed", dir);
    FILE *fp = fopen(path, "w");
    CHECK(fp != NULL, "malformed: file opened");
    if (fp == NULL)
    {
        return;
    }
    fputs("Good = https://example.com/good\n", fp);
    fputs("this line has no separator and no scheme\n", fp);
    fputs("Bad = notaurl\n", fp);
    fputs("Also Good = https://example.com/ok\n", fp);
    fclose(fp);

    playlist_t pl;
    CHECK(playlist_load("t-malformed", &pl) == PL_OK, "malformed: load still succeeds");
    CHECK(pl.count == 2, "malformed: invalid lines skipped, valid ones kept");
    if (pl.count == 2)
    {
        CHECK_STREQ(pl.songs[0].name, "Good", "malformed: first valid entry");
        CHECK_STREQ(pl.songs[1].name, "Also Good", "malformed: second valid entry");
    }
    playlist_free(&pl);
}

static void test_missing_playlist(void)
{
    playlist_t pl;
    CHECK(playlist_load("t-does-not-exist", &pl) == PL_ERR_NOT_FOUND,
          "missing: load reports not-found");
}

int main(void)
{
    char tpl[] = "/tmp/vyt-test-XXXXXX";
    char *tmp = mkdtemp(tpl);
    if (tmp == NULL)
    {
        perror("mkdtemp");
        return 1;
    }

    /* Point vyt's storage at an isolated scratch directory. */
    setenv("XDG_CONFIG_HOME", tmp, 1);

    test_dir_is_created();
    test_name_validation();
    test_create_duplicate_delete();
    test_list_sorted_and_filtered();
    test_song_add_and_list();
    test_song_add_validation();
    test_song_remove();
    test_load_formats();
    test_load_skips_malformed_lines();
    test_missing_playlist();

    rmtree(tmp);

    if (failures == 0)
    {
        printf("All %d checks passed.\n", checks);
        return 0;
    }

    fprintf(stderr, "%d/%d checks failed.\n", failures, checks);
    return 1;
}
