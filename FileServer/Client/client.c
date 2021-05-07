#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include "s_api.h"

/* TODO: this is a temporary solution */
#define SERVER_SOCKET_PATH "/mnt/c/Users/marco/Desktop/server_socket"

int fd_socket;

int main(int argc, char *argv[])
{

    struct timespec abs_time;
    int msec_waiting = 1500;

    abs_time.tv_sec = (time_t)(msec_waiting / 1000);
    abs_time.tv_nsec = (msec_waiting % 1000) * 1000000;

    if (openConnection(SERVER_SOCKET_PATH, 100, abs_time) == -1)
    {
        perror("Waiting Server time out (Client)\n");
        exit(EXIT_FAILURE);
    }

    /* TEMPORARY. THIS CODE NEED TO BE REPLACED */
    /* ----------------------- */
    write(fd_socket, "Hallo!", 7);
    printf("Write fatta (Client)\n");
    char buffer[30];

    read(fd_socket, buffer, 30);

    printf("Client got: %s\n", buffer);
    /* ----------------------- */

    if (closeConnection(SERVER_SOCKET_PATH) == -1)
    {
        perror("Closing Connection failed (Client)\n");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}