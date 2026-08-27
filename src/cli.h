#ifndef CLI_H
#define CLI_H

#include <stdio.h>

/* The mode the user requested on the command line. */
typedef enum
{
    MODE_NONE,   /* nothing selected */
    MODE_MUSIC,  /* -m URL  */
    MODE_VIDEO,  /* -v URL  */
    MODE_SEARCH, /* -s QUERY */
    MODE_HELP    /* -h */
} mode_t;

typedef struct
{
    mode_t mode;
    char *argument; /* URL for -m/-v, search query for -s, NULL for -h */
} options_t;

void cli_print_usage(FILE *stream);
void cli_print_help(void);

#endif /* CLI_H */
