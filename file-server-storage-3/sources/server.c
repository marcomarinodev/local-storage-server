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
 *   => Creare la struttura dati per lo storage (DONE)
 *   => Implementare le API
 *   => Gestire le funzioni del server
 *   => Fare in modo che quando i worker si mettono a lavoro
 *      la loro read sia sempre pronta
 *   => Gestire le signal (SIGINT, SIGQUIT, SIGHOP)
 *   => Creare dei test
 *   => Gestire i dettagli del progetto
 *   => Implementare i test finali
*/

/* pthread shared state */
/* it contains an array*/
static struct queue *pending_clients;
static icl_hash_t *storage_ht;

static pthread_mutex_t pending_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pending_clients_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t *workers_tid;

int main(int argc, char *argv[])
{
    check_argc(argc);

    Server_setup *server_setup = (Server_setup *)malloc(sizeof(Server_setup));

    /* socket info */
    int fd_socket, fd_client;
    struct sockaddr_un sa;
    int err;

    pending_clients = create_queue();

    /* in config_info[0] there is the server socket path */
    parse_config(argv[1], &(server_setup->config_info), CONFIG_ROWS);
    save_setup(&server_setup);
    storage_ht_init(&storage_ht, server_setup->max_storage, server_setup->max_files_instorage, 50);

    /**
     * Allocating workers
    */
    workers_tid = (pthread_t *)malloc(server_setup->n_workers * sizeof(pthread_t));
    for (int i = 0; i < server_setup->n_workers; i++)
    {
        Pthread_create(&workers_tid[i], NULL, &worker_func, NULL);
    }

    strncpy(sa.sun_path, server_setup->config_info[0], UNIX_PATH_MAX);

    for (int i = 0; i < 4; i++)
    {
        free(server_setup->config_info[i]);
    }
    free(server_setup->config_info);

    sa.sun_family = AF_UNIX;
    SYSCALL(fd_socket, socket(AF_UNIX, SOCK_STREAM, 0), "server socket");
    SYSCALL(err, bind(fd_socket, (struct sockaddr *)&sa, sizeof(sa)), "binding server socket");
    SYSCALL(err, listen(fd_socket, SOMAXCONN), "listening server");

    /**
     * Manager listen to new connections 
    */
    while (1)
    {
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

    clean_all(&pending_clients, &storage_ht, &workers_tid, &fd_client, &fd_socket);

    return 0;
}

/**
 * ----------------
 * ----------------
 * ----------------
 * ------MAIN------
 * ----------------
 * ----------------
 * */

void check_argc(int argc)
{
    if (argc == 1)
    {
        printf("You have to insert the configuration file\n");
        exit(EXIT_FAILURE);
    }
}

static void *worker_func(void *args)
{
    /* in this function the server will parse the client request */

    /* worker will read the request from fd_client */

    int fd_c;

    printf("--- thread %ld attivo ---\n", pthread_self());

    while (1)
    {
        Pthread_mutex_lock(&pending_clients_mutex);
        while (is_empty(pending_clients) == 1)
            Pthread_cond_wait(&pending_clients_cond, &pending_clients_mutex);

        fd_c = *(int *)(dequeue(pending_clients)->key);
        Pthread_mutex_unlock(&pending_clients_mutex);

        int message_len;

        int n = readn(fd_c, &message_len, sizeof(int));

        printf("\n**********************************************\n");
        printf("\nlunghezza della richiesta = %d\n", message_len);

        if (n == -1)
        {
            printf("error");
            exit(EXIT_FAILURE);
        }

        char *buffer = (char *)calloc(message_len, sizeof(char));

        if (!buffer)
        {
            perror("calloc");
            fprintf(stderr, "Memoria esaurita....\n");
            return NULL;
        }

        n = readn(fd_c, buffer, message_len * sizeof(char));

        for (int i = 0; i < message_len; i++)
        {
            printf("%c", buffer[i]);
        }

        char *copy = (char *)calloc(message_len, sizeof(char));
        strncpy(copy, buffer, message_len * sizeof(char));

        Receiving_request *parsed_request = malloc(sizeof(Receiving_request));

        ssize_t index = 0;

        char *token = strtok(buffer, "|");
        if (token != NULL)
            parsed_request->calling_client = atoi(token);

        index += strlen(token);

        token = strtok(NULL, "|");
        if (token != NULL)
        {
            parsed_request->cmd_type = atoi(token);
        }

        index += strlen(token);

        token = strtok(NULL, "|");
        if (token != NULL)
        {
            parsed_request->pathname = calloc(strlen(token) + 1, sizeof(char));
            strcpy(parsed_request->pathname, token);
        }

        index += strlen(token);

        token = strtok(NULL, "|");
        if (token != NULL)
        {
            parsed_request->flags = atoi(token);
        }

        index += strlen(token);

        token = strtok(NULL, "|");
        if (token != NULL)
        {
            parsed_request->size = atoi(token);
        }

        index += strlen(token) + 5; /* 5 = number of pipes before content */

        parsed_request->content = calloc(message_len - index + 1, sizeof(char));
        for (ssize_t i = index; i < message_len; i++)
        {
            // printf("%c", buffer[i]);
            strncat(parsed_request->content, &buffer[i], 1);
        }
        
        // token = strtok(NULL, "");
        // if (token != NULL)
        // {
        //     parsed_request->content = calloc(strlen(token) + 1, sizeof(char));
        //     strcpy(parsed_request->content, token);
        // }

        print_parsed_request(parsed_request);
        printf("\n**********************************************\n");

        char response[] = OPEN_SUCCESS;
        writen(fd_c, response, strlen(response) * sizeof(char));

        // int execution_result = execute_request(parsed_request, fd_c);

        // free(parsed_request->pathname);
        // free(parsed_request);
        // PIPE AL MANAGER DEL DESCRITTORE (4 BYTE)
    }

    free(args);

    return NULL;
}

int execute_request(Receiving_request *req, int fd_client)
{
    switch (req->cmd_type)
    {
    case OPEN_FILE_REQ:
    {
        // CHECKING FLAGS
        if (req->flags == NO_FLAGS)
        {
            Pthread_mutex_lock(&storage_mutex);
            int result = storage_open_file(req);
            Pthread_mutex_unlock(&storage_mutex);

            if (result == FALSE)
                write(fd_client, FAILED_FILE_SEARCH, 4);
            else
                write(fd_client, OPEN_SUCCESS, 4);

            return 0;
        }

        if (req->flags == O_CREATE)
        {
            /* first: if the file exists, open it and job is done */

            Pthread_mutex_lock(&storage_mutex);
            int result = storage_open_file(req);
            Pthread_mutex_unlock(&storage_mutex);

            if (result == TRUE)
            {
                write(fd_client, OPEN_SUCCESS, 4);
                return 0;
            }

            Pthread_mutex_lock(&storage_mutex);
            storage_init_file(req);
            Pthread_mutex_unlock(&storage_mutex);

            /* once we created the file, let's open it! */
            Pthread_mutex_lock(&storage_mutex);
            storage_open_file(req);
            Pthread_mutex_unlock(&storage_mutex);

            write(fd_client, O_CREATE_SUCCESS, 4);
        }
    }
    break;

    case WRITE_FILE_REQ:
    {
        int write_result;

        Pthread_mutex_lock(&storage_mutex);
        while ((write_result = storage_write_file(req)) == STRG_OVERFLOW)
        {
            // LRU call
        }
        Pthread_mutex_unlock(&storage_mutex);

        icl_hash_dump(stdout, storage_ht);

        if (write_result == 0)
            write(fd_client, WRITE_SUCCESS, 4);
    }
    break;
    default:
        break;
    }

    return 0;
}

// int storage_LRU_remove()
// {

// }

int storage_write_file(Receiving_request *req)
{
    FRecord *record_towrite_on = icl_hash_find(storage_ht, req->pathname);

    if ((storage_ht->current_size + record_towrite_on->size) > storage_ht->max_size)
        return STRG_OVERFLOW;

    time_t t = time(NULL);

    record_towrite_on->content = malloc(1 + (sizeof(char) * strlen(req->content)));

    record_towrite_on->last_edit = *localtime(&t);
    strncpy(record_towrite_on->content, req->content, strlen(req->content) + 1);

    storage_ht->current_size += record_towrite_on->size;

    return 0;
}

int storage_init_file(Receiving_request *req)
{
    /* we're shure that the file we want to add does not exist yet */
    FRecord *record_toadd = (FRecord *)malloc(sizeof(FRecord));

    record_toadd->pathname = (char *)malloc((sizeof(char) * strlen(req->pathname) + 1));

    if (record_toadd->pathname == NULL)
    {
        perror("storage_init_file memory error");
        exit(EXIT_FAILURE);
    }

    strncpy(record_toadd->pathname, req->pathname, strlen(req->pathname) + 1);
    record_toadd->size = req->size;
    record_toadd->is_open = FALSE;   /* in case of O_CREATE inside open file it will be edit by storage_open_file */
    record_toadd->is_locked = FALSE; /* in case of O_LOCK inside open file it will be edit by storage_lock_file */
    record_toadd->last_client = req->calling_client;
    record_toadd->content = req->content; /* initially is NULL, it can be update with writeFile */

    icl_hash_insert(storage_ht, record_toadd->pathname, record_toadd);

    return 0;
}

int storage_open_file(Receiving_request *req)
{
    FRecord *record = icl_hash_find(storage_ht, req->pathname);
    if (record == NULL) /* if the record does not exists (in server) */
    {

        return FALSE;
    }
    /* if the record exists, then set is_open flag as TRUE */
    record->is_open = TRUE;
    return TRUE;
}

void clean_all(struct queue **queue_ref, icl_hash_t **storage, pthread_t **workers_tid, int *fd_socket, int *fd_client)
{

    destroy_queue(pending_clients);

    icl_hash_destroy((*storage), free, free_file);

    free((*workers_tid));

    close((*fd_socket));
    close((*fd_client));
}

void free_file(void *record)
{
    FRecord *rec = (FRecord *)record;
    if (rec->pathname != NULL)
        free(rec->pathname);
    if (rec->content != NULL)
        free(rec->content);
}

void save_setup(Server_setup **setup)
{
    (*setup)->config_info[0][strlen((*setup)->config_info[0]) - 1] = '1';
    (*setup)->n_workers = atoi((*setup)->config_info[1]);
    (*setup)->max_storage = atoi((*setup)->config_info[2]);
    (*setup)->max_files_instorage = atoi((*setup)->config_info[3]);
}

void storage_ht_init(icl_hash_t **ht, long int max_size, int max_keys, int percentage)
{
    int ht_size = ((max_keys / 100) * percentage) + 1;

    (*ht) = icl_hash_create(max_size, ht_size, NULL, NULL);

    if (!(*ht))
    {
        fprintf(stderr, "HT Creation Error\n");
    }
}

void print_parsed_request(Receiving_request *parsed_request)
{
    printf("\n");
    printf("=> CALLING CLIENT     : %d\n", parsed_request->calling_client);
    printf("=> COMMAND TYPE       : %s\n", cmd_type_to_string(parsed_request->cmd_type));
    printf("=> PATHNAME           : %s\n", parsed_request->pathname);
    printf("=> FLAGS              : %d\n", parsed_request->flags);
    printf("=> FILE SIZE (IN DISK): %ld\n", parsed_request->size);
    printf("=> CONTENT            : %s\n", parsed_request->content);
    printf("\n");
}

char *cmd_type_to_string(int cmd_code)
{
    switch (cmd_code)
    {
    case 10:
        return "OPEN";
        break;
    case 11:
        return "READ";
        break;
    case 12:
        return "WRITE";
        break;
    case 13:
        return "APPEND";
        break;
    case 14:
        return "LOCK";
        break;
    case 15:
        return "UNLOCK";
        break;
    case 16:
        return "CLOSE";
        break;
    case 17:
        return "REMOVE";
        break;
    default:
        break;
    }

    return "UNKNOWN COMMAND";
}

ssize_t readn(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nread;

    nleft = n;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; /* error, return -1 */
            else
                break; /* error, return amount read so far */
        }
        else if (nread == 0)
            break; /* EOF */
        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); /* return >= 0 */
}

ssize_t writen(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;

    nleft = n;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; /* error, return -1 */
            else
                break; /* error, return amount written so far */
        }
        else if (nwritten == 0)
            break;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n - nleft); /* return >= 0 */
}