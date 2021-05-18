#if !defined(SERVER_H_)
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include "config_parser.h"
#include "pthread_custom.h"
#include "queue.h"
#include "ht.h"
#include "s_api.h"
#include "linked_list.h"


#define CONFIG_ROWS 4 /* rows in config.txt */
#define UNIX_PATH_MAX 108


/* Response codes */

#define CMD_EOF 41

/* Error types (Not fatal) */


/* Opening types */
#define TRUE 1
#define FALSE 0


typedef struct _server_setup
{
    int n_workers;
    int max_storage;
    int max_files_instorage;
    char **config_info;
} Server_setup;



void check_argc(int argc);

static void run_server(char *config_pathname);

void *worker_func(void *args);

void save_setup(Server_setup **setup);

void clean_all(pthread_t **workers_tid, int *fd_socket, int *fd_client);

void print_parsed_request(ServerRequest parsed_request);

char *cmd_type_to_string(int cmd_code);

ssize_t readn(int fd, void *ptr, size_t n);

ssize_t writen(int fd, void *ptr, size_t n);

#endif