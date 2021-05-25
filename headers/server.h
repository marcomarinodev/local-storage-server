#if !defined(SERVER_H_)
#define SERVER_H_

#define _POSIX_C_SOURCE 200112L
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
#include <stdarg.h>

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

/* Boolean representation */
#define TRUE 1
#define FALSE 0

#define SYSCALL_EXIT(name, r, sc, str, ...) \
    if ((r = sc) == -1)                     \
    {                                       \
        perror(#name);                      \
        int errno_copy = errno;             \
        print_error(str, __VA_ARGS__);      \
        exit(errno_copy);                   \
    }

typedef struct _server_setup
{
    int n_workers;
    int max_storage;
    int max_files_instorage;
    char **config_info;
} Server_setup;

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

static void *sigHandler(void *arg);

static void run_server(Server_setup *server_setup);

static void *worker_func(void *args);

int update_fds(fd_set set, int fd_num);

int select_lru_victims(size_t incoming_req_size, char *incoming_path);

int lru(HashTable ht, char *oldest_path, char *incoming_path);

char *conc(size_t size1, char const *str1, char const *str2);

void save_setup(Server_setup **setup);

void clean_all(pthread_t **workers_tid, int *fd_socket, int *fd_client);

void print_parsed_request(ServerRequest parsed_request);

void print_conn(Node *to_print);

char *cmd_type_to_string(int cmd_code);

ssize_t readn(int fd, void *ptr, size_t n);

ssize_t writen(int fd, void *ptr, size_t n);

#if !defined(BUFSIZE)
#define BUFSIZE 256
#endif

#if !defined(EXTRA_LEN_PRINT_ERROR)
#define EXTRA_LEN_PRINT_ERROR 512
#endif

/**
 * \brief Procedura di utilita' per la stampa degli errori
 *
 */
static inline void print_error(const char *str, ...)
{
    const char err[] = "ERROR: ";
    va_list argp;
    char *p = (char *)malloc(strlen(str) + strlen(err) + EXTRA_LEN_PRINT_ERROR);
    if (!p)
    {
        perror("malloc");
        fprintf(stderr, "FATAL ERROR nella funzione 'print_error'\n");
        return;
    }
    strcpy(p, err);
    strcpy(p + strlen(err), str);
    va_start(argp, str);
    vfprintf(stderr, p, argp);
    va_end(argp);
    free(p);
}

#endif