#if !defined(SERVER_H_)
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include "config_parser.h"
#include "pthread_custom.h"
#include "queue.h"
#include "icl_hash.h"

#define CONFIG_ROWS 4 /* rows in config.txt */
#define UNIX_PATH_MAX 108
#define MAX_CHARACTERS 100000

/* Response codes */
#define FAILED_FILE_SEARCH "404"
#define OPEN_SUCCESS "200"
#define WRITE_SUCCESS "230"
#define O_CREATE_SUCCESS "260"

/* Error types (Not fatal) */
#define STRG_OVERFLOW -2

/* Opening types */
#define TRUE 1
#define FALSE 0
#define NO_FLAGS -1
#define O_CREATE 1
#define O_LOCK 2

/* Types of command */
#define OPEN_FILE_REQ 10
#define READ_FILE_REQ 11
#define WRITE_FILE_REQ 12
#define APPEND_FILE_REQ 13
#define LOCK_FILE_REQ 14
#define UNLOCK_FILE_REQ 15
#define CLOSE_FILE_REQ 16
#define REMOVE_FILE_REQ 17

typedef struct _server_setup
{
    int n_workers;
    int max_storage;
    int max_files_instorage;
    char **config_info;
} Server_setup;

typedef struct _frecord
{
    /* metadata */
    char *pathname;
    size_t size;
    struct tm last_edit;
    int is_locked;
    int is_open;
    pid_t last_client;
    /* data */
    char *content;
} FRecord;

typedef struct receiving_request
{
    pid_t calling_client;
    int cmd_type;
    char *pathname;
    int flags;     /* -1 there is no flags */
    long int size; /* -1 there is no file to send */
    char *content;
} Receiving_request;

void check_argc(int argc);

static void *worker_func(void *args);

void free_file(void *record);

void save_setup(Server_setup **setup);

void clean_all(struct queue **queue_ref, icl_hash_t **storage, pthread_t **workers_tid, int *fd_socket, int *fd_client);

void storage_ht_init(icl_hash_t **ht, long int max_storage, int max_keys, int percentage);

int execute_request(Receiving_request *req, int fd_client);

/* storage internal operations */

int storage_open_file(Receiving_request *req);

int storage_init_file(Receiving_request *req);

int storage_write_file(Receiving_request *req);

int storage_lru_remove();

void print_parsed_request(Receiving_request *parsed_request);

char *cmd_type_to_string(int cmd_code);

ssize_t readn(int fd, void *ptr, size_t n);

ssize_t writen(int fd, void *ptr, size_t n);

#endif