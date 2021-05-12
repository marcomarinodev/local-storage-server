#include "server.h"

/**
 * TODO:
 *   => Sistemare il parsing del socket name (DONE)
 *   => Eliminare automaticamente il server_socket creato (DONE)
 *   => Creare la coda per l'attesa del worker da parte della richiesta client (DONE)
 *   => Gesitre la coda (DONE)
 *   => Creare le librerie dinamica (DONE)
 *   => Implementare l'interfaccia del client (DONE)
 *   => Mettere del codice di esempio nelle API (DONE)
 *   => Creare la struttura dati per lo storage
 *   => Implementare le API
 *   => Gestire le funzioni del server
 *   => Gestire le signal (SIGINT, SIGQUIT, SIGHOP)
 *   => Creare dei test
 *   => Gestire i dettagli del progetto
 *   => Implementare i test finali
*/

/* pthread shared state */
/* it contains an array*/
static struct queue *pending_clients;

static pthread_mutex_t pending_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pending_clients_cond = PTHREAD_COND_INITIALIZER;
static pthread_t *workers_tid;

int main(int argc, char *argv[])
{

    if (argc == 1)
    {
        printf("You have to insert the configuration file\n");
        exit(EXIT_FAILURE);
    }

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

    parse_config(argv[1], &config_info, CONFIG_ROWS);
    /* in config_info[0] there is the server socket path */
    config_info[0][strlen(config_info[0]) - 1] = '1';
    n_workers = atoi(config_info[1]);
    max_storage = atoi(config_info[2]);
    max_files_instorage = atoi(config_info[3]);

    /**
     * Allocating workers
    */
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

    /**
     * Manager listen to new connections 
    */
    while (1)
    {
        printf("(Server) in ascolto ...\n");
        SYSCALL(fd_client, accept(fd_socket, NULL, 0), "accepting connections");

        // FARE LA SELECT 
        // PRENDERE I DESCRITTORI PRONTI E INSERIRLI NELLA CODA

        Pthread_mutex_lock(&pending_clients_mutex);
        enqueue(pending_clients, &fd_client);
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

    printf("--- thread %ld attivo ---\n", pthread_self());

    while (1)
    {
        Pthread_mutex_lock(&pending_clients_mutex);
        while (is_empty(pending_clients) == 1)
            Pthread_cond_wait(&pending_clients_cond, &pending_clients_mutex);

        fd_c = *(int *)(dequeue(pending_clients)->key);
        Pthread_mutex_unlock(&pending_clients_mutex);

        read(fd_c, buffer, 30);

        printf("Worker with index %ld received: %s from client\n", pthread_self(), buffer);
        write(fd_c, "CiaoClient", 11);

        // PIPE AL MANAGER DEL DESCRITTORE (4 BYTE)
    }

    free(args);

    return NULL;
}