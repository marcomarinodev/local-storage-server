#include "config_parser.h"

int parse_config(Setup *setup_ref, char *config_path)
{
    char line[MAX_PATHNAME];
    FILE *fp = fopen(config_path, "r");

    if (fp == NULL)
    {
        perror("cannot open the config_path in parse_config");
        errno = PARSE_CONFIG_ERR;
        return -1;
    }

    if (setup_ref == NULL)
    {
        perror("setup_ref is NULL in parse_config");
        errno = PARSE_CONFIG_ERR;
        return -1;
    }

    /* server socket name */
    if (fscanf(fp, "%s\n", line) != 0)
        strcpy(setup_ref->server_socket_pathname, line);
    else
    {
        perror("cannot read server_socket line in config file");
        errno = PARSE_CONFIG_ERR;
        return -1;
    }

    /* number of workers */
    if (fscanf(fp, "%s\n", line) != 0)
    {
        setup_ref->n_workers = str_toint(line);

        if (setup_ref->n_workers <= 0)
        {
            perror("invalid number of workers");
            errno = PARSE_CONFIG_ERR;
            return -1;
        }
    }
    else
    {
        perror("cannot read number of workers line in config file");
        errno = PARSE_CONFIG_ERR;
        return -1;
    }

    /* server capacity */
    if (fscanf(fp, "%s\n", line) != 0)
    {
        setup_ref->max_storage = str_toint(line);

        if (setup_ref->max_storage < 0)
        {
            perror("invalid server capacity");
            errno = PARSE_CONFIG_ERR;
            return -1;
        }
    }
    else
    {
        perror("cannot read server capacity line in config file");
        errno = PARSE_CONFIG_ERR;
        return -1;
    }

    /* max number of files */
    if (fscanf(fp, "%s\n", line) != 0)
    {
        setup_ref->max_files_instorage = str_toint(line);

        if (setup_ref->max_files_instorage <= 0)
        {
            perror("invalid max number of files");
            errno = PARSE_CONFIG_ERR;
            return -1;
        }
    }
    else
    {
        perror("cannot read max number of files line in config file");
        errno = PARSE_CONFIG_ERR;
        return -1;
    }

    /* log pathname */
    if (fscanf(fp, "%s\n", line) != 0)
        strcpy(setup_ref->log_path, line);
    else
    {
        perror("cannot read log pathname line in config file");
        errno = PARSE_CONFIG_ERR;
        return -1;
    }

    fclose(fp);
    return 0;
}