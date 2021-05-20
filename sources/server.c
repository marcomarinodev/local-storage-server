#include "server.h"

/* SHARED STATE */
/* PENDING REQUESTS */
queue *pending_requests;
pthread_mutex_t pending_requests_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pending_requests_cond = PTHREAD_COND_INITIALIZER;

/* STORAGE */
HashTable *storage_ht;
pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Workers TID */
pthread_t *workers_tid;
pthread_attr_t *attributes;

/* SERVER SOCKET FILE DESCRIPTOR */
int fd_socket;

/* DESCRIPTORS SET */
fd_set set;
pthread_mutex_t descriptors_set = PTHREAD_MUTEX_INITIALIZER;

/* SIGNAL HANDLING */
int ending_all = 0;
int pfd[2];

int main(int argc, char *argv[])
{
    check_argc(argc);

    /* signal handling */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGHUP);

    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
    {
        fprintf(stderr, "\n<<<FATAL ERROR>>>\n");
        abort();
    }

    /* sigpipe ignore */
    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;

    if ((sigaction(SIGPIPE, &s, NULL)) == -1)
    {
        perror("<<<sigaction>>>");
        abort();
    }

    /* initialize the pipe in order to "unlock" the select
     * in case of SIGINT, SIGHUP, SIGQUIT
    */
    if (pipe(pfd) == -1)
    {
        perror("\n<<<PIPE CREATION ERROR>>>\n");
        exit(EXIT_FAILURE);
    }

    /* thread to handle signals */
    pthread_t sighandler_thread;

    Pthread_create(&sighandler_thread, NULL, sigHandler, &mask);

    run_server(argv[1]);

    /* waiting signal handler thread */
    printf("\n<<<Joining the signal thread>>>\n");
    /* clean data structures */
    Pthread_join(sighandler_thread, NULL);

    return 0;
}

static void *sigHandler(void *arg)
{
    sigset_t *set = (sigset_t *)arg;

    for (;;)
    {
        int sig;
        int r = sigwait(set, &sig);
        if (r != 0)
        {
            errno = r;
            perror("\n<<<FATAL ERROR 'sigwaitì>>>\n");
            return NULL;
        }

        switch (sig)
        {
        case SIGINT:
            ending_all = 1;

            close(pfd[1]);
            return NULL;
            break;
        case SIGQUIT:
            break;
        case SIGHUP:
            break;
        default:
            break;
        }
    }

    return NULL;
}

void spawn_thread(int index)
{

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGHUP);

    if (pthread_sigmask(SIG_BLOCK, &mask, &oldmask) != 0)
    {
        fprintf(stderr, "\n<<<FATAL ERROR>>>\n");
        close(fd_socket);
        return;
    }

    if (pthread_attr_init(&attributes[index]) != 0)
    {
        fprintf(stderr, "pthread_attr_init FALLITA\n");
        close(fd_socket);
        return;
    }

    /* detached mode */
    if (pthread_attr_setdetachstate(&attributes[index], PTHREAD_CREATE_JOINABLE) != 0)
    {
        fprintf(stderr, "pthread_attr_setdetachstate FALLITA\n");
        pthread_attr_destroy(&attributes[index]);
        close(fd_socket);
        return;
    }
    if (pthread_create(&workers_tid[index], &attributes[index], &worker_func, NULL) != 0)
    {
        fprintf(stderr, "pthread_create FALLITA");
        pthread_attr_destroy(&attributes[index]);
        close(fd_socket);
        return;
    }

    if (pthread_sigmask(SIG_SETMASK, &oldmask, NULL) != 0)
    {
        fprintf(stderr, "FATAL ERROR\n");
        close(fd_socket);
    }
}

static void run_server(char *config_pathname)
{
    struct sockaddr_un sa;

    sa.sun_family = AF_UNIX;
    int fd_client;
    int fd_num = 0;
    int fd;
    fd_set rdset;
    Server_setup *server_setup = (Server_setup *)malloc(sizeof(Server_setup));

    parse_config(config_pathname, &(server_setup->config_info), CONFIG_ROWS);
    save_setup(&server_setup);

    strcpy(sa.sun_path, server_setup->config_info[0]);

    fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    bind(fd_socket, (struct sockaddr *)&sa, sizeof(sa));
    listen(fd_socket, SOMAXCONN);

    /* max active fd is fd_num */
    if (fd_socket > fd_num)
        fd_num = fd_socket;

    FD_ZERO(&set);
    FD_SET(fd_socket, &set);
    /* register the read endpoint descriptor of the pipe */
    FD_SET(pfd[0], &set);

    /* update fd_num */
    if (pfd[0] > fd_num)
        fd_num = pfd[0];

    // INIT
    pending_requests = createQueue(sizeof(ServerRequest));

    save_setup(&server_setup);
    storage_ht = create_table(1 + (server_setup->max_files_instorage / 2), server_setup->max_storage);

    workers_tid = (pthread_t *)malloc(sizeof(pthread_t) * server_setup->n_workers);
    attributes = (pthread_attr_t *)malloc(sizeof(pthread_attr_t) * server_setup->n_workers);

    for (int i = 0; i < server_setup->n_workers; i++)
        spawn_thread(i);

    while (!ending_all)
    {
        printf("\n<<<inside select>>>\n");
        rdset = set; /* preparo maschera per select */
        if (select(fd_num + 1, &rdset, NULL, NULL, NULL) == -1)
        { /* gest errore */
            perror("select");
            exit(EXIT_FAILURE);
        }
        else
        { /* select OK */
            for (fd = 0; fd <= fd_num; fd++)
            {
                if (FD_ISSET(fd, &rdset))
                {
                    if (fd == fd_socket)
                    { /* sock connect pronto */
                        fd_client = accept(fd_socket, NULL, 0);
                        FD_SET(fd_client, &set);
                        if (fd_client > fd_num)
                            fd_num = fd_client;
                    }
                    else
                    { /* sock I/0 pronto */
                        // struttura richiesta
                        ServerRequest request;
                        // memset
                        memset(&request, 0, sizeof(request));
                        // parsing
                        int n_read = read(fd, &request, sizeof(ServerRequest));

                        if (n_read == 0)
                        {
                            FD_CLR(fd, &set);
                            //  fd_num = aggiorna(&set);
                            close(fd);
                        }
                        else
                        {
                            request.fd_cleint = fd;
                            printf(" --- INCOMING REQUEST ---\n");
                            print_parsed_request(request);

                            if (request.cmd_type == CMD_EOF)
                            { /* EOF client finito */
                                FD_CLR(fd, &set);
                                //  fd_num = aggiorna(&set);
                                close(fd);
                            }
                            else
                            {
                                // metti in coda
                                Pthread_mutex_lock_(&pending_requests_mutex);

                                enqueue(pending_requests, &request);
                                Pthread_cond_signal(&pending_requests_cond);

                                Pthread_mutex_unlock_(&pending_requests_mutex);
                            }
                        }
                    }
                }
            }
        }
    }

    printf("\n<<<SIGINT RECEIVED>>>\n");

    for (int i = 0; i < server_setup->n_workers; i++)
    {
        printf("\n<<<invio una signal su pending_request>>>\n");
        Pthread_cond_signal(&pending_requests_cond);
        sleep(1);
    }

    for (int i = 0; i < server_setup->n_workers; i++)
    {
        printf("\n<<<Joining %d worker thread>>>\n", i);
        Pthread_join(workers_tid[i], NULL);
    }

    for (int i = 0; i < 4; i++)
    {
        printf("\n<<<Freeing config_info row no.%d>>>\n", i);
        free(server_setup->config_info[i]);
    }

    free(server_setup->config_info);
    free(workers_tid);
    free(attributes);
    free(server_setup);
    destroyQueue(pending_requests);
    free_table(storage_ht);
}

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

    ServerRequest incoming_request;
    // memset(&incoming_request, 0, sizeof(ServerRequest));

    printf("--- thread %ld attivo ---\n", pthread_self());

    while (!ending_all)
    {
        Pthread_mutex_lock(&pending_requests_mutex, pthread_self(), "requests queue");

        if (isEmpty(pending_requests) == 1)
            Pthread_cond_wait(&pending_requests_cond, &pending_requests_mutex);

        if (ending_all == 1)
        {
            printf("\n<<<closing worker>>>\n");
            Pthread_mutex_unlock(&pending_requests_mutex, pthread_self(), "requests queue");
            pthread_exit(NULL);
        }

        dequeue(pending_requests, &incoming_request);

        Pthread_mutex_unlock(&pending_requests_mutex, pthread_self(), "requests queue");

        printf(" --- WORKER REQUEST ---\n");
        print_parsed_request(incoming_request);

        switch (incoming_request.cmd_type)
        {
        case OPEN_FILE_REQ:
        {
            Response response;
            memset(&response, 0, sizeof(Response));

            if (incoming_request.flags == (O_CREATE | O_LOCK))
            {

                time_t now = time(0);

                /* preparing the request */
                FRecord new;
                new.is_locked = 0;
                new.is_open = 1;
                new.last_client = incoming_request.calling_client;

                new.last_edit = now;
                new.size = incoming_request.size;
                sprintf(new.pathname, "%s", incoming_request.pathname);

                new.content = malloc(1 + (sizeof(char) * incoming_request.size));
                memcpy(new.content, incoming_request.content, incoming_request.size);

                Pthread_mutex_lock(&storage_mutex, pthread_self(), "storage");

                FRecord *rec = ht_search(storage_ht, incoming_request.pathname);

                if (rec != NULL) /* file already exists in storage */
                {
                    Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");

                    Pthread_mutex_lock(&(rec->lock), pthread_self(), rec->pathname);

                    if (rec->is_locked == FALSE)
                    {
                        if (rec->is_open == TRUE) /* if the file already exists and it's already open */
                        {
                            /* if the client that wants to open the file already open is not the client that opened the file before */
                            if (rec->last_client != incoming_request.calling_client)
                            {
                                response.code = IS_ALREADY_OPEN;
                            }
                            else
                                response.code = OPEN_SUCCESS;
                        }
                        else
                        {
                            rec->is_open = TRUE;
                            response.code = OPEN_SUCCESS;
                        }
                    }
                    else
                    {
                        /* case when the file is already locked */
                        /* case when the client that locked this file is trying to open ig */
                        if (rec->last_client == incoming_request.calling_client)
                        {
                            rec->is_open = TRUE;
                            response.code = OPEN_SUCCESS;
                        }
                        else
                        {
                            /* file is locked by another client */
                            response.code = FILE_IS_LOCKED;
                        }
                    }

                    free(new.content);

                    Pthread_mutex_unlock(&(rec->lock), pthread_self(), rec->pathname);
                }
                else /* file does not exists */
                {
                    if (incoming_request.size <= storage_ht->capacity)
                    { /* case when storage can contain the file sent in request */

                        pthread_mutex_init(&(new.lock), NULL);
                        new.is_open = TRUE;
                        new.is_locked = TRUE;
                        new.is_new = TRUE;
                        new.last_client = incoming_request.calling_client;

                        ht_insert(storage_ht, incoming_request.pathname, new, incoming_request.size);
                        /* decrementing the n. of elements in storage because the file needs to be filled before */
                        /* decrementing the size of the element in storage because the file needs to be filled before */
                        storage_ht->count--;
                        storage_ht->file_size -= incoming_request.size;

                        free(new.content);

                        response.code = O_CREATE_SUCCESS;
                    }
                    else
                    {
                        free(new.content);
                        response.code = STRG_OVERFLOW;
                        printf("\n<<<STORAGE IS EMPTY => FILE IS TOO BIG>>>");
                    }
                }

                Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
            }

            printf("writing to fd: %d\n", incoming_request.fd_cleint);
            writen(incoming_request.fd_cleint, &response, sizeof(response));
        }
        break;

        case WRITE_FILE_REQ:
        {
            Response response;
            memset(&response, 0, sizeof(Response));

            Pthread_mutex_lock(&storage_mutex, pthread_self(), "storage");

            time_t now = time(0);
            FRecord *rec = ht_search(storage_ht, incoming_request.pathname);

            if (rec != NULL)
            {
                if (rec->is_open == TRUE && rec->is_locked == TRUE)
                {

                    while ((incoming_request.size + storage_ht->file_size) > storage_ht->capacity)
                    {
                        /* LRU handling ... */
                        /* deleting the oldest file until the server can host the incoming file */
                        printf("\n<<<Calling LRU>>>\n");

                        char oldest_pathname[MAX_PATHNAME];

                        int is_selected;
                        if ((is_selected = lru(storage_ht, oldest_pathname)) == -1)
                        {
                            /* it should never happen because we know that the storage can contain the incoming file */
                            printf("\n<<<STORAGE IS EMPTY => FILE IS TOO BIG>>>");
                        }

                        printf("\n<<<VICTIM SELECTED: %s>>>\n", oldest_pathname);

                        ht_delete(storage_ht, oldest_pathname);
                    }

                    storage_ht->file_size += rec->size;
                    /* when I really insert the file, increment the counter */
                    storage_ht->count++;

                    Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");

                    Pthread_mutex_lock(&(rec->lock), pthread_self(), rec->pathname);

                    rec->is_locked = FALSE;
                    
                    rec->last_edit = now;
                    if (rec->content == NULL)
                        rec->content = malloc(1 + (sizeof(char) * incoming_request.size));

                    memcpy(rec->content, incoming_request.content, incoming_request.size);
                    

                    Pthread_mutex_unlock(&(rec->lock), pthread_self(), rec->pathname);

                    response.code = WRITE_SUCCESS;
                }
                else
                {
                    Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
                    /* before you do a write operation, you need to open the file */
                    printf("\n<<<The client must open the file using O_CREATE | O_LOCK flags>>>\n");
                    response.code = WRITE_FAILED;
                }
            }
            else /* the file does not exists, so I am going to return a write file error */
            {
                Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
                /* write file error due to the miss of the file that the client want to write on */
                /* before you do a write operation, you need to open the file */
                response.code = WRITE_FAILED;
            }

            printf("writing to fd: %d\n", incoming_request.fd_cleint);
            writen(incoming_request.fd_cleint, &response, sizeof(response));
        }
        break;

        case READ_FILE_REQ:
        {
            Response response;
            memset(&response, 0, sizeof(Response));

            Pthread_mutex_lock(&storage_mutex, pthread_self(), "storage");

            printf("\n<<<STORAGE LOCKED (READ FILE REQUEST)(Search)>>>\n");
            FRecord *rec = ht_search(storage_ht, incoming_request.pathname);

            time_t now = time(0);

            if (rec != NULL)
            {
                if (rec->is_locked == FALSE)
                {
                    /* sending the size of the content, in order to read exactly that size */
                    if (rec->content != NULL)
                    {
                        Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");

                        Pthread_mutex_lock(&(rec->lock), pthread_self(), rec->pathname);

                        response.code = READ_SUCCESS;
                        rec->last_edit = now;
                        response.content_size = rec->size;

                        strncpy(response.path, incoming_request.pathname, strlen(incoming_request.pathname) + 1);
                        memcpy(response.content, rec->content, rec->size);

                        Pthread_mutex_unlock(&(rec->lock), pthread_self(), rec->pathname);
                    }
                    else
                    {
                        Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
                        printf("Rec->content is null\n");
                        response.code = FAILED_FILE_SEARCH;
                    }
                }
                else
                {
                    Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
                }
            }
            else
            {
                Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
                response.code = FAILED_FILE_SEARCH;
            }

            printf("writing to fd: %d\n", incoming_request.fd_cleint);
            writen(incoming_request.fd_cleint, &response, sizeof(response));
        }
        break;

        case READN_FILE_REQ:
        {
            Response response;
            memset(&response, 0, sizeof(response));

            int n = incoming_request.flags;

            Pthread_mutex_lock(&storage_mutex, pthread_self(), "storage");
            if (n < 0 || n > storage_ht->count)
            {
                response.code = storage_ht->count;
            }
            else
                response.code = n;

            writen(incoming_request.fd_cleint, &response, sizeof(response));

            int n_to_read = response.code;

            /* now that I sent the number of files to read, I am going to send
             * n_to_read files content
            */
            int k = 0;
            for (int i = 0; i < storage_ht->size; i++)
            {
                if (k < n_to_read)
                {
                    Response file_response;
                    memset(&file_response, 0, sizeof(file_response));

                    response.code = READ_SUCCESS;
                    Ht_item *rec = storage_ht->items[i];

                    if (rec != NULL)
                    {
                        if (rec->value.is_locked == FALSE)
                        {
                            time_t now = time(0);

                            rec->value.last_edit = now;
                            response.content_size = rec->value.size;

                            printf("sizeof(rec->content) = %ld\n", rec->value.size);

                            strncpy(response.path, rec->key, strlen(rec->key) + 1);
                            memcpy(response.content, rec->value.content, rec->value.size);

                            printf("sending file...\n");
                            writen(incoming_request.fd_cleint, &response, sizeof(response));
                            k++;
                        }
                    }
                }
                else
                    break;
            }

            printf("\n<<<%d files succesfully sent!>>>\n", k);

            Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
        }
        break;

        case APPEND_FILE_REQ:
        {
        }
        break;

        case REMOVE_FILE_REQ:
        {
            Response response;
            memset(&response, 0, sizeof(response));

            Pthread_mutex_lock(&storage_mutex, incoming_request.calling_client, "storage");
            FRecord *rec = ht_search(storage_ht, incoming_request.pathname);

            if (rec != NULL)
            {

                if (rec->is_locked == TRUE)
                {
                    /* two cases: calling client is the client who locked */
                    if (rec->last_client == incoming_request.calling_client)
                    {
                        printf("\n>>>(DEBUG) The client who's trying to remove the locked file is the client who locked the file<<<\n");
                        printf("\n<<<REMOVING %s>>>\n", incoming_request.pathname);
                        ht_delete(storage_ht, incoming_request.pathname);
                        response.code = REMOVE_FILE_SUCCESS;
                    }
                    else
                        response.code = FILE_IS_LOCKED;
                }
                else
                {
                    /* if it is not locked the client can remove the item */
                    printf("\n<<<REMOVING %s>>>\n", incoming_request.pathname);
                    ht_delete(storage_ht, incoming_request.pathname);
                    response.code = REMOVE_FILE_SUCCESS;
                }
            }
            else
                response.code = FAILED_FILE_SEARCH;

            Pthread_mutex_unlock(&storage_mutex, incoming_request.calling_client, "storage");

            writen(incoming_request.fd_cleint, &response, sizeof(response));
        }
        break;

        case CLOSE_FILE_REQ:
        {
            Response response;
            memset(&response, 0, sizeof(response));

            Pthread_mutex_lock(&storage_mutex, incoming_request.calling_client, "storage");
            FRecord *rec = ht_search(storage_ht, incoming_request.pathname);

            if (rec != NULL)
            {
                Pthread_mutex_unlock(&storage_mutex, incoming_request.calling_client, "storage");
                time_t now = time(0);

                Pthread_mutex_lock(&(rec->lock), pthread_self(), rec->pathname);
                if (rec->is_open == TRUE)
                {
                    /* split in two cases */
                    /* first case: this file is locked */
                    if (rec->is_locked == TRUE)
                    {
                        /* we have to check that the last client (the client that made the lock) */
                        /* if the client that makes the closing request is the last client => is legal */
                        if (rec->last_client == incoming_request.calling_client)
                        {
                            /* legal ==> closing the file */
                            rec->is_open = FALSE;
                            rec->is_new = FALSE;
                            response.code = CLOSE_FILE_SUCCESS;
                        }
                        else
                        {
                            /* if is not the client who made the lock ==> illegal, response code is FILE_IS_LOCKED */
                            response.code = FILE_IS_LOCKED;
                        }
                    }

                    /* second case: this file is not locked so we can close it */
                    rec->is_open = FALSE;
                    rec->is_new = FALSE;
                    response.code = CLOSE_FILE_SUCCESS;
                }
                else
                {
                    /* the file is not open, so is already closed */
                    response.code = IS_ALREADY_CLOSED;
                }
                Pthread_mutex_unlock(&(rec->lock), pthread_self(), rec->pathname);
            }
            else
            {
                Pthread_mutex_lock(&storage_mutex, incoming_request.calling_client, "storage");
                /* the file we want to close does not exist */
                response.code = FAILED_FILE_SEARCH;
            }

            /* send the response via API */
            writen(incoming_request.fd_cleint, &response, sizeof(response));
        }

        break;
        default:
            break;
        }

        print_table(storage_ht);

        // Pthread_mutex_lock(&descriptors_set, pthread_self(), "descriptors set");
        // FD_SET(incoming_request.fd_cleint, &set);
        // Pthread_mutex_unlock(&descriptors_set, pthread_self(), "descriptors set");
    }

    return NULL;
}

void clean_all(pthread_t **workers_tid, int *fd_socket, int *fd_client)
{
    destroyQueue(pending_requests);
    free_table(storage_ht);

    free((*workers_tid));

    close((*fd_socket));
    close((*fd_client));
}

void save_setup(Server_setup **setup)
{
    (*setup)->config_info[0][strlen((*setup)->config_info[0]) - 1] = '1';
    (*setup)->n_workers = atoi((*setup)->config_info[1]);
    (*setup)->max_storage = atoi((*setup)->config_info[2]);
    (*setup)->max_files_instorage = atoi((*setup)->config_info[3]);
}

void print_parsed_request(ServerRequest parsed_request)
{
    printf("\n");
    printf("=> CALLING CLIENT     : %d\n", parsed_request.calling_client);
    printf("=> COMMAND TYPE       : %s\n", cmd_type_to_string(parsed_request.cmd_type));
    printf("=> PATHNAME           : %s\n", parsed_request.pathname);
    printf("=> FLAGS              : %d\n", parsed_request.flags);
    printf("=> FILE SIZE (IN DISK): %ld\n", parsed_request.size);
    if (parsed_request.content != NULL)
        printf("=> CONTENT            : %s\n", parsed_request.content);
    printf("\n");
}

char *cmd_type_to_string(int cmd_code)
{
    if (cmd_code == 10)
        return "OPEN";
    if (cmd_code == 11)
        return "READ";
    if (cmd_code == 12)
        return "WRITE";
    if (cmd_code == 13)
        return "APPEND";
    if (cmd_code == 14)
        return "LOCK";
    if (cmd_code == 15)
        return "UNLOCK";
    if (cmd_code == 16)
        return "CLOSE";
    if (cmd_code == 17)
        return "REMOVE";
    if (cmd_code == 18)
        return "READN";

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