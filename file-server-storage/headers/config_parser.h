#if !defined(CONFIG_PARSER_H_)
#define CONFIG_PARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define SYSCALL(r, c, e) \
    if ((r = c) == -1)   \
    {                    \
        perror(e);       \
        exit(errno);     \
    }


void parse_config(char * pathname, char *** parsed_config_info, int config_rows);

#endif