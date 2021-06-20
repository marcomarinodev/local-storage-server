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

/**
 * Struct containing updated info about server status
 * 
 * @param capacity: it represents the maximum number of bytes that
 *                  the server can hold  
 * @param actual_capacity: current number of bytes inside the server
 * @param max_files: maximum number of files the server can hold
 * @param actual_max_files: current number of files inside the server
 * 
*/
typedef struct _status
{
    int capacity;
    int actual_capacity;
    int max_files;
    int actual_max_files;
} serv_status;

/**
 * Check the number of arguments passed 
 * 
 * @param argc: argc passed inside the main function
 * 
 * @return: exit(EXIT_FAILURE) if the number of arguments is 0
*/
void check_argc(int argc);

/**
 * It spawns a thread filling a specific cell with index inside
 * a couple of arrays: the first one contains the tid of the spawned 
 * thread, while the second one contains the attributes of the corresponding
 * thread.
 * 
 * @param index: position to put the information about tid and its attributes
*/
void spawn_thread(int index);

/**
 * Create the log file inside a directory specified in config_logpath (relative)
 * 
 * @param config_logpath: directory to send the log file
 * 
 * @return: returns 0 on success, fatal errors otherwise
*/
int log_init(char *config_logpath);

/**
 * Writes formatted string into a log file previously created with log_init function
 * 
 * @param format: string format
 * @param ...: variables to substitute in case of % in format parameter
*/
void print_log(const char *format, ...);

/**
 * Thread that catches SIGQUIT, SIGINT, SIGHUP signals and notifies the server the occurence
 * 
 * @param arg: arguments used to pass the signal mask from the main thread
*/
static void *sigHandler(void *arg);

/**
 * Function that run the manager inside the main thread. What run_server do:
 * - managing new connection and incoming requests from clients
 * - listening to possible signals from the sigHandler thread.
 * - listening to possible workers (when a worker notifies the server to re-listen 
 *   to a specific client descriptor)
 * - managing when a client closed the connection with the server 
 * 
 * @param server_setup: Setup structure used 
*/
static void run_server(Setup *server_setup);

/**
 * Worker thread that executes the requests (taken from a queue of requests).
 * (It pops the request from the pending requests queue)
 * Based on what is the request command code (coming from the server API), the worker
 * performs operations inside the storage in mutual exclusion (file m.e or entire storage m.e).
 * When a worker thread finished its job, it has to notify the server to re-listen to the 
 * serverd client descriptor.
 * 
 * @param args: formal args, not used in this context 
*/
static void *worker_func(void *args);

/**
 * Deletes all files selected as victims by lru function. Files are deleted from the storage, but
 * they're temporary saved in return value, in order to perform the sending of expelled files (as a
 * possible request from client)
 * 
 * @param incoming_req_size: it has to know the dimension of the file to insert or more in general
 *                          the additional size of the file (in case of appendToFile request)
 * @param incoming_path: used from lru in order to avoid the elimination of the file we want to add/extend
 * @param n_removed_files: pointer to an integer that will be changed to the number of remove file by lru,
 *                       basically the cardinality of the return array
 * @return: returns an array containing all removed files
*/
FRecord *select_lru_victims(size_t incoming_req_size, char *incoming_path, int *n_removed_files);

/**
 * Finds the victim path relative to the least recently used file in storage
 * 
 * @param ht: hashtable to search on
 * @param incoming_path: path of the file we want to add, in order to deselect it from the possible victims
 * 
 * @return: victim path in storage 
*/
char *lru(HashTable ht, char *incoming_path);

/**
 * Helper function used to append the new content to the old content of a file
 * 
 * @param size1: old file size
 * @param str1: old string
 * @param str2: new content string
 * 
 * @return: concatenated old string + new content string
*/
char *conc(size_t size1, char const *str1, char const *str2);

/**
 * Convert a command code into a string
 *
 * @param cmd_code: command code
 * 
 * @return: string version of the command code 
*/
char *cmd_type_to_string(int cmd_code);

#endif