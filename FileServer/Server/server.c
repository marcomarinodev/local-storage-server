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
#include "util.h"
#define CONFIG_ROWS 4 /* rows in config.txt */
#define UNIX_PATH_MAX 108

/**
 * TODO:
 *   => Sistemare il parsing del socket name (DONE)
 *   => Eliminare automaticamente il server_socket creato (DONE)
 *   => Creare la coda per l'attesa del worker da parte della richiesta client (DONE)
 *   => Gesitre la coda (DONE)
 *   => Implementare l'interfaccia del client
 *   => Mettere del codice di esempio nelle API
 *   => Creare la struttura dati per lo storage
 *   => Implementare le API
 *   => Gestire le signal (SIGINT, SIGQUIT, SIGHOP)
 *   => Creare dei test
 *   => Gestire i dettagli del progetto
 *   => Implementare i test finali
*/

static void *worker_func(void *args);

/* pthread shared state */
/* it contains an array*/
static struct queue *pending_clients;

static pthread_mutex_t pending_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pending_clients_cond = PTHREAD_COND_INITIALIZER;
static pthread_t *workers_tid;

struct pthread_arg_struct
{
    int fd_c;
    int worker_index;
};

int main(int argc, char *argv[])
{

    /* config variables */
    int n_workers;
    int max_storage;
    int max_files_instorage;
    char **config_info;

    /* socket info */
    int fd_socket, fd_client;
    struct sockaddr_un sa;
    int err;

    /* pending clients queue */
    pending_clients = create_queue();

    parse_config("config.txt", &config_info, CONFIG_ROWS);
    /* in config_info[0] there is the server socket path */
    config_info[0][strlen(config_info[0]) - 1] = '1';
    n_workers = atoi(config_info[1]);
    max_storage = atoi(config_info[2]);
    max_files_instorage = atoi(config_info[3]);

    
    workers_tid = (pthread_t *)malloc(n_workers * sizeof(pthread_t));
    for (int i = 0; i < n_workers; i++)
    {
        Pthread_create(&workers_tid[i], NULL, &worker_func, NULL);
        Pthread_detach(workers_tid[i]);
    }

    /* FIX THE FORMAT OF PARSED SOCK NAME !!! */
    strncpy(sa.sun_path, config_info[0], UNIX_PATH_MAX);
    // strncpy(sa.sun_path, "/mnt/c/Users/marco/Desktop/server_socket", UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    SYSCALL(fd_socket, socket(AF_UNIX, SOCK_STREAM, 0), "server socket");
    SYSCALL(err, bind(fd_socket, (struct sockaddr *)&sa, sizeof(sa)), "binding server socket");
    SYSCALL(err, listen(fd_socket, SOMAXCONN), "listening server");

    /* dispatcher */
    while (1)
    {
        printf("(Server) in ascolto ...\n");
        SYSCALL(fd_client, accept(fd_socket, NULL, 0), "accepting connections");

        Pthread_mutex_lock(&pending_clients_mutex);
        enqueue(pending_clients, fd_client);
        Pthread_cond_signal(&pending_clients_cond);
        Pthread_mutex_unlock(&pending_clients_mutex);
    }

    /* ENDING SERVER */
    /* DURANTE L'ESECUZIONE CI SARANNO DELLE FREE MANCANTI, QUESTO E'
       A CAUSA DELLA INTERRUZIONE NON GESTITA AL BLOCCO DI ACCEPT */
    for (int i = 0; i < 4; i++)
    {
        free(config_info[i]);
    }

    destroy_queue(pending_clients);

    free(config_info);
    free(workers_tid);

    close(fd_socket);
    close(fd_client);

    return 0;
}

static void *worker_func(void *args)
{
    /* in this function the server will parse the client request */

    /* worker will read the request from fd_client */
    char buffer[30];
    int fd_c;

    while (1)
    {
        Pthread_mutex_lock(&pending_clients_mutex);
        while (is_empty(pending_clients) == 1)
            Pthread_cond_wait(&pending_clients_cond, &pending_clients_mutex);

        fd_c = (dequeue(pending_clients)->key);
        Pthread_mutex_unlock(&pending_clients_mutex);

        read(fd_c, buffer, 30);

        printf("Worker with index %ld received: %s from client\n", pthread_self(), buffer);
        write(fd_c, "CiaoClient", 11);
    }

    free(args);

    return NULL;
}