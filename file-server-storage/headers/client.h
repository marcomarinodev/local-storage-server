#if !defined(CLIENT_H_)
#define CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>

#include "s_api.h"
#include "queue.h"

#define SERVER_SOCKET_PATH "/tmp/server_sock1"
#define NOPTIONS 1

void check_argc(int arg_c);

#endif