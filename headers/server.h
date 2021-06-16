#if !defined(SERVER_H_)
#define SERVER_H_

#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <stdarg.h>
#include <dirent.h>

#include "consts.h"
#include "doubly_ll.h"
#include "linked_list.h"
#include "config_parser.h"
#include "pthread_custom.h"
#include "queue.h"
#include "ht.h"
#include "s_api.h"

#define CONFIG_ROWS 5 /* rows in config.txt */

typedef struct _status
{
    int capacity;
    int actual_capacity;
    int max_files;
    int actual_max_files;
} serv_status;

typedef struct _c
{
    int fd;
} _conn;

void check_argc(int argc);

void spawn_thread();

int log_init(char *config_logpath);

void print_log(const char *format, ...);

static void *sigHandler(void *arg);

static void run_server(Setup *server_setup);

static void *worker_func(void *args);

int update_fds(fd_set set, int fd_num);

FRecord *select_lru_victims(size_t incoming_req_size, char *incoming_path, int *n_removed_files);

char *lru(HashTable ht, char *incoming_path);

char *conc(size_t size1, char const *str1, char const *str2);

void clean_all(pthread_t **workers_tid, int *fd_socket, int *fd_client);

void print_conn(Node *to_print);

char *cmd_type_to_string(int cmd_code);

#endif