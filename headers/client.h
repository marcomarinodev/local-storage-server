#if !defined(CLIENT_H_)
#define CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <dirent.h>
#include <fcntl.h>

#include "linked_list.h"
#include "s_api.h"

#define HELP_EXIT -1
#define MISSING_ARG -2
#define INVALID_OPT -3
#define NAN_INPUT_ERROR -4
#define INVALID_NUMBER_INPUT_ERROR -5
#define INCONSISTENT_INPUT_ERROR -6
#define MISSING_SOCKET_NAME -7
#define DIRNAME_NOT_FOLDER -8
#define FS_ERR -9
#define INEXISTENT_FILE -10
#define NOSUCHFILE_ORDIR -11

#define LISTED_ARG 0
#define OPTIONAL_ARG 1
#define CONFIG_ARG 2
#define MAXFILENAME 300
#define MAX_FILE_SIZE 100000
#define EXAUSTIVE_READ -1

#define PATH_CHECK(pathname)    \
    if (pathname == NULL)       \
    {                           \
        return INEXISTENT_FILE; \
    }

/* request struct */
typedef struct request
{
    char code;
    char *arguments;
} Request;

typedef struct client_setup
{
    char *socket_pathname;
    char *dirname_buffer;
    int req_time_interval;
    int op_log;
} Client_setup;

/* core functions */
Client_setup apply_setup(LList config_commands);

int perform(Client_setup setup, LList *request_commands);

int lsR(const char dirname[], int *n);

void check_argc(int argcount);

int _getopt(LList *configs, LList *reqs, int argcount, char **_argv);

int validate(LList configs, LList requests);

char **parse_comma_args(char *arguments);

int openConnection(const char *sockname, int msec, const struct timespec abstime);

int closeConnection(const char *sockname);

int fd_is_valid(int fd);

void manage_request_option(char **opt_id, int opt, Request **_req, LList *reqs, char *_optarg);

void manage_config_option(char **opt_id, char **opt_arg_value, int opt, LList *configs, char *_optarg);

void clean_options(LList *configs, LList *requests);

/* series of printers*/
void print_help(char **_argv);

void print_config_node(Node *to_print);

void print_request_node(Node *to_print);

void print_setup(Client_setup setup);

void print_requests(LList *request_commands);

#endif