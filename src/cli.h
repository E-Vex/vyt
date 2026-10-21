#ifndef CLI_H
#define CLI_H

#include <stdio.h>

/* The mode the user requested on the command line.
 * Note the cli_ prefix: a bare "mode_t" would collide with the POSIX type
 * of the same name the moment a translation unit includes <sys/types.h>.
 *
 * The `vyt playlist ...` subcommand tree is not a mode: it is a positional
 * command tree that main.c hands to the playlist module before the options
 * are ever looked at. */
typedef enum
{
    MODE_NONE,     /* nothing selected */
    MODE_MUSIC,    /* -m URL  */
    MODE_VIDEO,    /* -v URL  */
    MODE_SEARCH,   /* -s QUERY */
    MODE_DOWNLOAD, /* -d URL  */
    MODE_HELP,     /* -h */
    MODE_VERSION   /* -V */
} cli_mode_t;

typedef struct
{
    cli_mode_t mode;

    /* The URL for -m/-v/-d (already normalized), the search query for -s,
     * or NULL for -h/-V and for the `playlist` subcommand tree.
     *
     * cli_parse() always allocates this string, so the caller owns it and
     * must free() it. */
    char *argument;
} options_t;

void cli_print_usage(FILE *stream);
void cli_print_help(void);
void cli_print_version(void);

int cli_parse(int argc, char **argv, options_t *opts);

#endif /* CLI_H */
