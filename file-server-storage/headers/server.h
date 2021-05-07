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

#define CONFIG_ROWS 4 /* rows in config.txt */
#define UNIX_PATH_MAX 108

static void *worker_func(void *args);

#endif