#include "config_parser.h"

/**
 * Il file config è composto da CONFIG_ROWS righe con un ordine preciso:
 * - socket path [0]
 * - numero di workers [1]
 * - dimensione massima del server [2] (in bytes)
 * - numero massimo di file [3]
*/

void parse_config(char *pathname, char ***parsed_config_info, int config_rows)
{

    int max_n_of_characters = 80;
    char line_buffer[max_n_of_characters];
    FILE *fp = fopen(pathname, "r");

    if (fp == NULL)
    {
        perror("fopen parse_config");
        exit(EXIT_FAILURE);
    }

    *parsed_config_info = malloc(config_rows * sizeof(char *));

    if (!*parsed_config_info)
    {
        fprintf(stderr, "Error allocating array of strings (config) \n");
        return;
    }

    for (int i = 0; i < config_rows; i++)
    {
        /* Get the first line of the file */
        if (fgets(line_buffer, max_n_of_characters, fp) == NULL)
        {
            fprintf(stderr, "Error getting config line '%s'\n", pathname);
            return;
        }

        /* line_buffer => parsed_config_info */
        (*parsed_config_info)[i] = malloc(80 * sizeof(char));

        if (!(*parsed_config_info)[i])
        {
            fprintf(stderr, "Error allocating config string\n");
            exit(EXIT_FAILURE);
        }

        strncpy((*parsed_config_info)[i], line_buffer, strlen(line_buffer) + 1);
    }

    fclose(fp);
}