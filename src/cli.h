#ifndef CLI_H
#define CLI_H

#include <stdio.h>

/* The mode the user requested on the command line.
 * Note the cli_ prefix: a bare "mode_t" would collide with the POSIX type
 * of the same name the moment a translation unit includes <sys/types.h>. */
typedef enum
{
    MODE_NONE,    /* nothing selected */
    MODE_MUSIC,   /* -m URL  */
    MODE_VIDEO,   /* -v URL  */
    MODE_SEARCH,  /* -s QUERY */
    MODE_HELP,    /* -h */
    MODE_VERSION, /* -V */
    MODE_PLAYLIST /* `vyt playlist ...` subcommand tree */
} cli_mode_t;

typedef struct
{
    cli_mode_t mode;
    char *argument; /* URL for -m/-v, search query for -s, NULL for -h/-V */
} options_t;

void cli_print_usage(FILE *stream);
void cli_print_help(void);
void cli_print_version(void);

int cli_parse(int argc, char **argv, options_t *opts);

#endif /* CLI_H */
