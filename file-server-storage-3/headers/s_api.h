#if !defined(S_API_H_)
#define S_API_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#define UNIX_PATH_MAX 108
#define NO_FLAGS -1
#define O_CREATE 1
#define O_LOCK 2

#define MAX_CHARACTERS 100000
#define MAX_REQUEST 200

/* Response codes */
#define FAILED_FILE_SEARCH "404"
#define OPEN_SUCCESS "200"
#define WRITE_SUCCESS "230"
#define O_CREATE_SUCCESS "260"

#define OPEN_FILE_REQ 10
#define READ_FILE_REQ 11
#define WRITE_FILE_REQ 12
#define APPEND_FILE_REQ 13
#define LOCK_FILE_REQ 14
#define UNLOCK_FILE_REQ 15
#define CLOSE_FILE_REQ 16
#define REMOVE_FILE_REQ 17

#define SYSCALL(r, c, e) \
    if ((r = c) == -1)   \
    {                    \
        perror(e);       \
        exit(errno);     \
    }

typedef struct request_tosend
{
    pid_t calling_client;
    int cmd_type;
    char *pathname;
    int flags;     /* -1 there is no flags */
    long int size; /* -1 there is no file to send (in bytes) */
    char *content;
} Request_tosend;

struct pthread_arg_struct
{
    int fd_c;
    int worker_index;
};

/**
 * It opens AF_UNIX connection to the socket named sockname. If the server does not
 * accept the request immediately, the client try to reconnect after msec. If the server
 * did not accept the request of the client in the allotted time (abstime), it returns -1 (errno).
 * Otherwise the connection is stable and it returns 0.
*/
int openConnection(const char *sockname, int msec, const struct timespec abstime);

/**
 * It closes AF_UNIX connection to the socket named sockname. It returns 0 in success, -1 
 * otherwise.
*/
int closeConnection(const char *sockname);

int openFile(const char *pathname, int flags);

int closeFile(const char *pathname);

int writeFile(const char *pathname, const char *dirname);

int readFile(const char *pathname, void **buf, size_t *size);

/* TODO: - IMPLEMENT THESE FUNCTIONS
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

int removeFile(const char* pathname);
*/
int send_req(Request_tosend *r);

long int find_size(const char *pathname);

char **parse_pipe_args(char *arguments, int n_args, int lengths[]);

ssize_t readn(int fd, void *ptr, size_t n);

ssize_t writen(int fd, void *ptr, size_t n);

#endif