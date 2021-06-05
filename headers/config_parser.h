#if !defined(CONFIG_PARSER_H_)
#define CONFIG_PARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "error_codes.h"
#include "constraints.h"
#include "utility.h"

#define SYSCALL(r, c, e) \
    if ((r = c) == -1)   \
    {                    \
        perror(e);       \
        exit(errno);     \
    }

typedef struct s_setup
{
    int n_workers;
    int max_storage;
    int max_files_instorage;
    char server_socket_pathname[MAX_PATHNAME];
    char log_path[MAX_PATHNAME];
} Setup;

int parse_config(Setup *setup_ref, char *config_path);

#endif