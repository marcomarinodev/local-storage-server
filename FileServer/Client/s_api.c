#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#define UNIX_PATH_MAX 108

#define SYSCALL(r, c, e) \
    if ((r = c) == -1)   \
    {                    \
        perror(e);       \
        exit(errno);     \
    }

extern int fd_socket;

int openConnection(const char *sockname, int msec, const struct timespec abstime)
{
    // FD_SOCKET DOVREBBE ESSERE QUI? OPPURE DOVREBBE ESSERE CHIAMATO GLOBALMENTE?
    // int fd_socket
    struct sockaddr_un sa;
    time_t start_t, end_t;
    long diff_t;

    strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    SYSCALL(fd_socket, socket(AF_UNIX, SOCK_STREAM, 0), "client socket");

    /* timer starts */
    printf("Timer starts! (Client)\n");
    time(&start_t);

    while (connect(fd_socket, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        if (errno == ENOENT)
        {
            time(&end_t);
            diff_t = difftime(end_t, start_t);

            if (diff_t > abstime.tv_nsec)
            {
                /* Connection timed out */
                errno = ETIMEDOUT;
                return -1;
            }
            sleep(msec);
        }
    }

    printf("connected at '%s'\n", sockname);

    return 0;
}

int closeConnection(const char *sockname)
{
    printf("Trying to close socket for connection at '%s'\n", sockname);
    return close(fd_socket);
}