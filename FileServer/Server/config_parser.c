#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "util.h"

/**
 * Il file config è composto da CONFIG_ROWS righe con un ordine preciso:
 * - socket path [0]
 * - numero di workers [1]
 * - dimensione massima del server [2] (in MB)
 * - numero massimo di file [3]
*/

void parse_config(char *pathname, char ***parsed_config_info, int config_rows)
{

    char *line_buffer = NULL;
    size_t line_buffer_size = 0;
    ssize_t line_size;
    FILE *fp;
    SYSCALL_FILE(fp, fopen(pathname, "r"), "fopen config");

    *parsed_config_info = malloc(config_rows * sizeof(char *));

    if (!*parsed_config_info)
    {
        fprintf(stderr, "Error allocating array of strings (config) \n");
        return;
    }

    for (int i = 0; i < config_rows; i++)
    {
        /* Get the first line of the file */
        if ((line_size = getline(&line_buffer, &line_buffer_size, fp)) == -1)
        {
            fprintf(stderr, "Error getting config line '%s'\n", pathname);
            return;
        }

        if (line_size == 0)
        {
            printf("An empty row occurred in config!\n");
            exit(EXIT_FAILURE);
        }

        /* line_buffer => parsed_config_info */
        (*parsed_config_info)[i] = malloc(((line_size) * sizeof(char)));

        if (!(*parsed_config_info)[i])
        {
            fprintf(stderr, "Error allocating config string\n");
            exit(EXIT_FAILURE);
        }

        strncpy((*parsed_config_info)[i], line_buffer, line_size + 1);
    }

    free(line_buffer);
    line_buffer = NULL;
    fclose(fp);
}