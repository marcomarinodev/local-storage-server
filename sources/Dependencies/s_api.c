#include "s_api.h"
#include <time.h>

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
                printf("TIME OUT\n");
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

int writeFile(const char* pathname, const char* dirname)
{
    printf("writing on abs path: %s\n", pathname);
    return 0;
}

int readFile(const char* pathname, void** buf, size_t* size)
{
    printf("reading abs path: %s\n", pathname);
    return 0;
}