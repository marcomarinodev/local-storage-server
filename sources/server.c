#include "server.h"

/* Pending requests */
static queue *pending_requests;
static pthread_mutex_t pending_requests_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pending_requests_cond = PTHREAD_COND_INITIALIZER;

/* Storage HT */
static HashTable storage_ht;
static pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;

static serv_status server_stat;
static pthread_mutex_t server_stat_mtx = PTHREAD_MUTEX_INITIALIZER;

/* Workers TID */
static pthread_t *workers_tid;
static pthread_attr_t *attributes;

/* Server socket file descriptor */
static int fd_socket;

/* fds active */
static fd_set active_set;

/* Master - Worker Pipe */
static int mwpipe[2];
static pthread_mutex_t mwpipe_mutex = PTHREAD_MUTEX_INITIALIZER;

/* true -> a worker can write on pipe */
static int can_pipe = TRUE;

/* workers who have done their request */
static pthread_cond_t workers_done = PTHREAD_COND_INITIALIZER;

/* SIGNAL HANDLING */
volatile int ending_all = 0;

static int pfd[2];
static pthread_mutex_t spipe_mutex = PTHREAD_MUTEX_INITIALIZER;

static int is_sigquit = FALSE;
static int is_sighup = FALSE;

/* list of client_fd elements */
static struct dd_Node *active_connections;
static int dim = 0;
static pthread_mutex_t ac_mutex = PTHREAD_MUTEX_INITIALIZER;

/* log file */
FILE *log = NULL;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
    active_connections = NULL;

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

    Setup *server_setup = (Setup *)malloc(sizeof(Setup));

    if (parse_config(server_setup, argv[1]) == -1)
    {
        printf("%d", errno);
        exit(EXIT_FAILURE);
    }

    log_init(server_setup->log_path);

    pending_requests = createQueue(sizeof(ServerRequest));

    server_stat.actual_capacity = 0;
    server_stat.actual_max_files = 0;
    server_stat.capacity = server_setup->max_storage;
    server_stat.max_files = server_setup->max_files_instorage;

    ht_create(&storage_ht, 1 + (server_stat.capacity / 2));

    workers_tid = (pthread_t *)malloc(sizeof(pthread_t) * server_setup->n_workers);
    attributes = (pthread_attr_t *)malloc(sizeof(pthread_attr_t) * server_setup->n_workers);

    /* initialize the pipe in order to "unlock" the select
     * in case of SIGINT, SIGHUP, SIGQUIT
    */
    if (pipe(pfd) == -1)
    {
        perror("\n<<<PIPE CREATION ERROR>>>\n");
        exit(EXIT_FAILURE);
    }

    if (pipe(mwpipe) == -1)
    {
        perror("\n<<<PIPE CREATION ERROR>>>\n");
        exit(EXIT_FAILURE);
    }

    /* thread to handle signals */
    pthread_t sighandler_thread;

    Pthread_create(&sighandler_thread, NULL, sigHandler, &mask);

    run_server(server_setup);

    /* waiting signal handler thread */
    printf("\n<<<Joining the signal thread>>>\n");
    /* clean data structures */
    Pthread_join(sighandler_thread, NULL);

    return 0;
}

static void *sigHandler(void *arg)
{
    sigset_t *set = (sigset_t *)arg;
    int sig, r, res;

    r = sigwait(set, &sig);

    if (r != 0)
    {
        errno = r;
        perror("\n<<<FATAL ERROR 'sigwait>>>\n");
        exit(EXIT_FAILURE);
    }

    /* writing into the pipe which is the signal code */
    pthread_mutex_lock(&spipe_mutex);

    /* delegating what to do to the manager */
    if ((res = writen(pfd[1], &sig, sizeof(int))) == -1)
    {
        perror("\n<<<FATAL ERROR 'writing into pipe error'>>>\n");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_unlock(&spipe_mutex);

    return (void *)NULL;
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

    /* joinable mode */
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

static void run_server(Setup *server_setup)
{
    struct sockaddr_un sa;

    int fd_client;
    int fd_num = 0;
    int fd, sig;
    ServerRequest new_request;
    fd_set rdset;

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, server_setup->server_socket_pathname);

    fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    // unlink(sa.sun_path);
    bind(fd_socket, (struct sockaddr *)&sa, sizeof(sa));
    listen(fd_socket, SOMAXCONN);

    FD_ZERO(&active_set);
    FD_SET(fd_socket, &active_set);

    /* max active fd is fd_num */
    if (fd_socket > fd_num)
        fd_num = fd_socket;

    /* register the read endpoint descriptor of the pipe */
    FD_SET(mwpipe[0], &active_set);

    if (mwpipe[0] > fd_num)
        fd_num = mwpipe[0];

    /* register the read endpoint descriptor of the pipe */
    FD_SET(pfd[0], &active_set);

    /* update fd_num */
    if (pfd[0] > fd_num)
        fd_num = pfd[0];

    for (int i = 0; i < server_setup->n_workers; i++)
        spawn_thread(i);

    printf("\n<<<Listen to requests, fd_socket = %d>>>\n", fd_socket);

    /* in order to manage SIGHUP, i cannot get out from select, otherwise i'm not going be able
     * to listen to new requests from pending connected clients */

    /* SIGINT/SIGQUIT is simple, we only have to close all of connections inside the LList,
     * resume workers until they exit and free their tcbs with pthread_join, and of course
     * clean all of the objects inside the heap
    */
    while (ending_all != TRUE)
    {
        pthread_mutex_lock(&ac_mutex);
        rdset = active_set; /* preparo maschera per select */
        pthread_mutex_unlock(&ac_mutex);

        struct timeval tv = {1, 0};

        if (select(fd_num + 1, &rdset, NULL, NULL, &tv) == -1)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }
        else
        {
            // printf("$$$ select (fd_set dim = %d):\n", fd_num);
            // for (fd = 0; fd <= fd_num; fd++)
            // {
            //     if (FD_ISSET(fd, &rdset))
            //         printf(" %d ", fd);
            //     else
            //         printf("0");
            // }

            // printf("\n");

            // printf("$$$ ciclo bitmap descrittori (fd_set dim = %d):\n", fd_num);
            for (fd = 0; fd <= fd_num; fd++)
            {
                if (FD_ISSET(fd, &rdset))
                    printf(" %d ", fd);
                else
                    printf("0");

                /* descriptor is ready */
                // printf("$$$ controllo indice %d[set=%d]\n", fd, FD_ISSET(fd, &rdset));
                if (FD_ISSET(fd, &rdset))
                {
                    /* listen socket ready ==> accept is not blocking */
                    if (fd == fd_socket)
                    {
                        fd_client = accept(fd_socket, NULL, 0);
                        printf("\nil client con fd = %d si sta connettendo\n", fd_client);

                        pthread_mutex_lock(&ac_mutex);

                        FD_SET(fd_client, &active_set);

                        /* append the new connection to active list */
                        d_append(&active_connections, fd_client);

                        dim++;

                        pthread_mutex_unlock(&ac_mutex);

                        if (fd_client > fd_num)
                            fd_num = fd_client;

                        printf("\n((( %d is connected )))\n", fd_client);
                    }
                    else
                    { /* sock I/0 ready */

                        pthread_mutex_lock(&spipe_mutex);

                        if (fd == pfd[0])
                        {
                            int res;
                            /* reading signal code from pipe */
                            if ((res = readn(pfd[0], &sig, sizeof(int))) == -1)
                            {
                                perror("<<<FATAL ERROR while reading into the pipe>>>");
                                exit(EXIT_FAILURE);
                            }

                            FD_CLR(pfd[0], &active_set);
                            pthread_mutex_unlock(&spipe_mutex);

                            /* case if signal is SIGINT or SIGQUIT */
                            /* what we have to do is simply close all active connections
                             * and terminate the workers properly
                            */
                            if (sig == SIGINT || sig == SIGQUIT)
                            {
                                ending_all = TRUE;
                                /* taking the lock of the requests before waking up asleep workers */
                                pthread_mutex_lock(&pending_requests_mutex);

                                is_sigquit = TRUE;
                                pthread_cond_broadcast(&pending_requests_cond);

                                pthread_mutex_unlock(&pending_requests_mutex);
                                break;
                            }
                            else if (sig == SIGHUP)
                            {
                                /* if the signal is SIGHUP I have to close the server socket
                                 * and clear its bit inside the bitmap. We also need the mutex
                                 * of active connections d_list in order to check if it is free.
                                */
                                pthread_mutex_lock(&ac_mutex);

                                is_sighup = TRUE;

                                printf("SIGHUP occurred!\n");

                                close(fd_socket);
                                FD_CLR(fd_socket, &active_set);

                                /* if there are some active connections, then the server must complete their
                                 * requests, so we do another select loop,
                                */
                                if (dim == 0)
                                {
                                    /* do what we did for SIGQUIT/SIGHUP */
                                    printf("active connections list is empty\n");
                                    if (active_connections != NULL)
                                        printf("FAILED INVARIANT => %d\n", active_connections->data);

                                    /* taking the lock of the requests before waking up asleep workers */
                                    pthread_mutex_lock(&pending_requests_mutex);

                                    is_sigquit = TRUE;

                                    pthread_cond_broadcast(&pending_requests_cond);

                                    pthread_mutex_unlock(&pending_requests_mutex);
                                    pthread_mutex_unlock(&ac_mutex);

                                    ending_all = TRUE;

                                    break;
                                }

                                pthread_mutex_unlock(&ac_mutex);
                            }
                        }
                        else
                        {
                            pthread_mutex_unlock(&spipe_mutex);

                            /* master worker pipe checking */
                            pthread_mutex_lock(&mwpipe_mutex);

                            /* pipe reading (re-listen to a client) */
                            if (fd == mwpipe[0])
                            {
                                int res, new_desc;
                                if ((res = readn(mwpipe[0], &new_desc, sizeof(int))) == -1)
                                {
                                    perror("Read mw pipe error");
                                    exit(EXIT_FAILURE);
                                }

                                printf("\n%d needs to be listened again\n", new_desc);

                                /* if a worker needs to write on pipe, it needs to find this variable at TRUE */
                                can_pipe = 1;

                                /* re-listen to the socket */
                                FD_SET(new_desc, &active_set);

                                /* update maximum fd */
                                if (new_desc > fd_num)
                                    fd_num = new_desc;

                                Pthread_cond_signal(&workers_done);
                                pthread_mutex_unlock(&mwpipe_mutex);
                            }
                            else
                            { /* normal request */
                                pthread_mutex_unlock(&mwpipe_mutex);
                                memset(&new_request, 0, sizeof(ServerRequest));

                                int n_read = readn(fd, &new_request, sizeof(ServerRequest));

                                /* EOF in fd */
                                if (n_read == 0)
                                {
                                    close(fd);

                                    pthread_mutex_lock(&ac_mutex);
                                    /* remove it from bitmap */
                                    FD_CLR(fd, &active_set);

                                    /* remove it from active connections */
                                    d_delete_with_key(&active_connections, fd);
                                    dim--;

                                    if (dim == 0 && is_sighup == TRUE)
                                    {
                                        /* no connections anymore, shutdown the server */
                                        printf("\nit was the last request, quitting\n");
                                        ending_all = TRUE;

                                        pthread_mutex_lock(&pending_requests_mutex);

                                        is_sigquit = TRUE;

                                        pthread_cond_broadcast(&pending_requests_cond);
                                        pthread_mutex_unlock(&pending_requests_mutex);

                                        pthread_mutex_unlock(&ac_mutex);

                                        break;
                                    }

                                    printf("\n%d disconnected, printing the connections list\n", fd);

                                    pthread_mutex_unlock(&ac_mutex);
                                }
                                else
                                {
                                    new_request.fd_cleint = fd;

                                    /* suspending manager to listen this descriptor */
                                    pthread_mutex_lock(&ac_mutex);
                                    FD_CLR(fd, &active_set);
                                    pthread_mutex_unlock(&ac_mutex);

                                    printf(" --- INCOMING REQUEST ---\n");
                                    print_parsed_request(new_request);

                                    // metti in coda
                                    Pthread_mutex_lock_(&pending_requests_mutex, "pending requests");

                                    enqueue(pending_requests, &new_request);
                                    Pthread_cond_signal(&pending_requests_cond);

                                    Pthread_mutex_unlock_(&pending_requests_mutex, "pending requests");
                                }
                            }
                        }
                    }
                }
            }

            printf("\n");
        }
    }

    printf("\n<<<SIGNAL RECEIVED>>>\n");

    /* close all connections inside the active_connections LList */
    struct dd_Node *current = active_connections;
    if (current != NULL)
    {
        struct dd_Node *prev = current;

        while (current != NULL)
        {
            current = current->next;
            d_delete_node(&active_connections, prev);
            prev = current;
        }

        /* delete last node */
        d_delete_node(&active_connections, prev);
    }
    else
        printf("\n<<<all of connections were closed (maybe from a SIGHUP)>>>\n");

    printf("\n\t(***) STORAGE (***)\n");
    printf("\tcapacity = %d out of %d\n", server_stat.actual_capacity, server_stat.capacity);
    printf("\tfiles     = %d out of %d\n", server_stat.actual_max_files, server_stat.max_files);
    ht_print(storage_ht);
    printf("\n\t(*****************)\n");

    /* in caso di sighup/sigint */
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

    free(workers_tid);
    free(attributes);
    free(server_setup);
    destroyQueue(pending_requests);
    ht_free(storage_ht);
}

void print_conn(Node *to_print)
{
    _conn con = *(_conn *)to_print->data;

    printf("\tactive fd: %d\n", con.fd);
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

    while (TRUE)
    {
        Pthread_mutex_lock(&pending_requests_mutex, pthread_self(), "requests queue");

        if (isEmpty(pending_requests) == 1)
            Pthread_cond_wait(&pending_requests_cond, &pending_requests_mutex);

        if (is_sigquit == TRUE)
        {
            printf("\n<<<closing worker>>>\n");
            Pthread_mutex_unlock(&pending_requests_mutex, pthread_self(), "requests queue");
            break;
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

                Pthread_mutex_lock(&storage_mutex, pthread_self(), "storage");

                if (ht_exists(storage_ht, incoming_request.pathname)) /* file already exists in storage */
                {
                    Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");

                    response.code = FILE_ALREADY_EXISTS;
                }
                else /* file does not exists */
                {
                    if (incoming_request.size <= server_stat.capacity)
                    { /* case when storage can contain the file sent in request */

                        FRecord *new = (FRecord *)malloc(sizeof(FRecord));
                        memset(new, 0, sizeof(FRecord));

                        char *key = (char *)malloc(MAX_PATHNAME * sizeof(char));

                        memset(key, 0, MAX_PATHNAME);

                        strncpy(new->pathname, incoming_request.pathname, MAX_PATHNAME);
                        strncpy(key, new->pathname, MAX_PATHNAME);

                        pthread_mutex_init(&(new->lock), NULL);
                        new->is_open = TRUE;
                        new->is_locked = TRUE;
                        new->is_new = TRUE;
                        new->is_victim = FALSE;
                        new->size = incoming_request.size;
                        new->last_client = incoming_request.calling_client;

                        ht_insert(&storage_ht, new, key);

                        response.code = O_CREATE_SUCCESS;

                        pthread_mutex_lock(&server_stat_mtx);

                        // server_stat.actual_capacity += incoming_request.size;
                        server_stat.actual_max_files++;

                        pthread_mutex_unlock(&server_stat_mtx);

                        Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
                    }
                    else
                    {
                        Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
                        response.code = STRG_OVERFLOW;
                        printf("\n<<<STORAGE IS EMPTY => FILE IS TOO BIG>>>");
                    }
                }
            }

            printf("writing to fd: %d\n", incoming_request.fd_cleint);
            writen(incoming_request.fd_cleint, &response, sizeof(response));
        }
        break;

        case APPEND_FILE_REQ:
        {
            /* the append operation is atomic, so we need to take the lock of the server */
            Pthread_mutex_lock(&storage_mutex, pthread_self(), "storage");
            Response response;
            memset(&response, 0, sizeof(Response));

            /* searching for the file */
            if (ht_exists(storage_ht, incoming_request.pathname))
            {
                /* if it exists then get it */
                FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);

                if (rec->is_locked == FALSE)
                {
                    if (incoming_request.size > server_stat.capacity)
                    {
                        response.code = STRG_OVERFLOW;
                    }
                    else
                    {
                        /* in order to deselect rec from the LRU search ==> rec->is_new = TRUE */
                        rec->is_new = TRUE;

                        /* lru check */
                        int n_to_eject;
                        FRecord *files_to_send = select_lru_victims(incoming_request.size, incoming_request.pathname, &n_to_eject);

                        response.code = n_to_eject;
                        printf("£££ sending response.code = %d\n", response.code);
                        writen(incoming_request.fd_cleint, &response, sizeof(response));

                        /* now the server knows which files to send */
                        /* so at the end of a single cycle a writen will be made in order to
                     * send the ejected file
                    */

                        for (int i = 0; i < n_to_eject; i++)
                        {
                            Response file_response;
                            memset(&file_response, 0, sizeof(file_response));

                            file_response.content_size = files_to_send[i].size;

                            printf("sizeof(rec->content) = %ld\n", files_to_send[i].size);

                            strncpy(file_response.path, files_to_send[i].pathname, strlen(files_to_send[i].pathname) + 1);
                            memcpy(file_response.content, files_to_send[i].content, files_to_send[i].size);

                            printf("sending file...\n");
                            writen(incoming_request.fd_cleint, &file_response, sizeof(file_response));
                        }

                        /* Having space to append... */
                        time_t now = time(NULL);
                        size_t old_size = rec->size;

                        rec->is_locked = FALSE;
                        rec->is_new = FALSE;
                        rec->last_edit = now;
                        rec->size += incoming_request.size;

                        if (rec->content == NULL)
                        {
                            rec->content = malloc(1 + (sizeof(char) * incoming_request.size));
                            memcpy(rec->content, incoming_request.content, incoming_request.size);
                        }
                        else
                        {
                            printf("<<<appending>>>\n");
                            char *new_content = conc(old_size, rec->content, incoming_request.content);
                            free(rec->content);
                            rec->content = malloc(1 + (sizeof(char) * (incoming_request.size + old_size)));
                            memcpy(rec->content, new_content, old_size + incoming_request.size);
                            free(new_content);
                        }

                        server_stat.actual_capacity += incoming_request.size;

                        response.code = APPEND_FILE_SUCCESS;
                    }
                }
            }

            writen(incoming_request.fd_cleint, &response, sizeof(response));
            Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
        }
        break;

        case WRITE_FILE_REQ:
        {
            Response response;
            memset(&response, 0, sizeof(Response));

            pthread_mutex_lock(&storage_mutex);

            time_t now = time(0);

            if (ht_exists(storage_ht, incoming_request.pathname))
            {
                FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);

                pthread_mutex_unlock(&storage_mutex);

                printf("file opened with O_CREATE exists and its path is: %s\n", rec->pathname);

                if (rec->is_open == TRUE && rec->is_locked == TRUE)
                {
                    printf("attempting to write on file opened with O_CREATE\n");
                    /* use response.code as n_to_eject */

                    /* lru check */
                    int n_to_eject;
                    FRecord *files_to_send = select_lru_victims(incoming_request.size, incoming_request.pathname, &n_to_eject);

                    response.code = n_to_eject;

                    printf("£££ sending response.code = %d\n", response.code);
                    writen(incoming_request.fd_cleint, &response, sizeof(response));

                    /* now the server knows which files to send */
                    /* so at the end of a single cycle a writen will be made in order to
                     * send the ejected file
                    */

                    for (int i = 0; i < n_to_eject; i++)
                    {
                        Response file_response;
                        memset(&file_response, 0, sizeof(file_response));

                        file_response.content_size = files_to_send[i].size;

                        printf("sizeof(rec->content) = %ld\n", files_to_send[i].size);

                        strncpy(file_response.path, files_to_send[i].pathname, strlen(files_to_send[i].pathname) + 1);
                        memcpy(file_response.content, files_to_send[i].content, files_to_send[i].size);

                        printf("sending file...\n");
                        writen(incoming_request.fd_cleint, &file_response, sizeof(file_response));

                        if (files_to_send[i].content != NULL)
                            free(files_to_send[i].content);
                    }

                    if (files_to_send != NULL)
                        free(files_to_send);

                    Pthread_mutex_lock(&(rec->lock), pthread_self(), rec->pathname);

                    rec->is_locked = FALSE;

                    rec->last_edit = now;
                    if (rec->content == NULL)
                        rec->content = malloc(1 + (sizeof(char) * incoming_request.size));

                    memset(rec->content, 0, 1 + incoming_request.size);
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
                response.code = FAILED_FILE_SEARCH;
            }

            printf("writing to fd: %d\n", incoming_request.fd_cleint);

            response.code = WRITE_SUCCESS;

            writen(incoming_request.fd_cleint, &response, sizeof(response));
        }
        break;

        case READ_FILE_REQ:
        {
            Response response;
            memset(&response, 0, sizeof(Response));

            Pthread_mutex_lock(&storage_mutex, pthread_self(), "storage");

            time_t now = time(0);

            if (ht_exists(storage_ht, incoming_request.pathname))
            {
                FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);

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
            if (n < 0 || n > server_stat.actual_max_files)
            {
                response.code = server_stat.actual_max_files;
            }
            else
                response.code = n;

            writen(incoming_request.fd_cleint, &response, sizeof(response));

            int n_to_read = response.code;

            /* now that I sent the number of files to read, I am going to send
             * n_to_read files content
            */
            int k = 0;
            /* for each row in table */
            for (int row = 0; row < storage_ht.size; row++)
            {
                /* for each file record in this row */
                for (int index = 0; index < storage_ht.lists[row].length; index++)
                {
                    if (k < n_to_read)
                    {
                        Response file_response;
                        memset(&file_response, 0, sizeof(file_response));

                        FRecord *rec = (FRecord *)LL_get(storage_ht.lists[row], index);

                        if (rec != NULL)
                        {
                            if (rec->is_locked == FALSE)
                            {
                                response.content_size = rec->size;

                                printf("sizeof(rec->content) = %ld\n", rec->size);

                                strncpy(response.path, rec->pathname, strlen(rec->pathname) + 1);
                                memcpy(response.content, rec->content, rec->size);

                                printf("sending file...\n");
                                response.code = READ_SUCCESS;
                                writen(incoming_request.fd_cleint, &response, sizeof(response));
                                k++;
                            }
                        }
                    }
                    else
                        break;
                }

                if (k >= n_to_read)
                    break;
            }

            printf("\n<<<%d files succesfully sent!>>>\n", k);
            Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");
        }

        break;

        case REMOVE_FILE_REQ:
        {
            Response response;
            memset(&response, 0, sizeof(response));

            Pthread_mutex_lock(&storage_mutex, incoming_request.calling_client, "storage");

            if (ht_exists(storage_ht, incoming_request.pathname))
            {
                FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);

                if (rec->is_locked == TRUE)
                {
                    /* two cases: calling client is the client who locked */
                    if (rec->last_client == incoming_request.calling_client)
                    {
                        printf("\n>>>(DEBUG) The client who's trying to remove the locked file is the client who locked the file<<<\n");
                        printf("\n<<<REMOVING %s>>>\n", incoming_request.pathname);

                        /* update server status */
                        pthread_mutex_lock(&server_stat_mtx);
                        
                        server_stat.actual_max_files--;
                        server_stat.actual_capacity -= rec->size;

                        pthread_mutex_unlock(&server_stat_mtx);

                        ht_delete(&storage_ht, incoming_request.pathname);

                        response.code = REMOVE_FILE_SUCCESS;
                    }
                    else
                        response.code = FILE_IS_LOCKED;
                }
                else
                {
                    /* if it is not locked the client can remove the item */
                    printf("\n<<<REMOVING %s>>>\n", incoming_request.pathname);

                    /* update server status */
                    pthread_mutex_lock(&server_stat_mtx);

                    server_stat.actual_max_files--;
                    server_stat.actual_capacity -= rec->size;

                    pthread_mutex_unlock(&server_stat_mtx);

                    ht_delete(&storage_ht, incoming_request.pathname);
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

            if (ht_exists(storage_ht, incoming_request.pathname))
            {
                FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);
                Pthread_mutex_unlock(&storage_mutex, incoming_request.calling_client, "storage");

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
                            rec->is_locked = FALSE;
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
                Pthread_mutex_unlock(&storage_mutex, incoming_request.calling_client, "storage");
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

        /* writing into the pipe in order to tell the manager to re-listen to this fd */
        pthread_mutex_lock(&mwpipe_mutex);

        while (can_pipe == 0)
            pthread_cond_wait(&workers_done, &mwpipe_mutex);

        can_pipe = 0;

        int res;
        printf("\n<<<riascolto al fd %d>>>\n", incoming_request.fd_cleint);
        if ((res = writen(mwpipe[1], &(incoming_request.fd_cleint), sizeof(int))) == -1)
        {
            perror("Writing on pipe error");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_unlock(&mwpipe_mutex);

        printf("wrote on pipe\n");
    }

    pthread_exit((void *)NULL);
}

char *conc(size_t size1, char const *str1, char const *str2)
{
    size_t const l2 = strlen(str2);

    char *result = malloc(size1 + l2 + 1);
    if (!result)
        return result;
    memcpy(result, str1, size1);
    memcpy(result + size1, str2, l2 + 1);
    return result;
}

FRecord *select_lru_victims(size_t incoming_req_size, char *incoming_path, int *n_removed_files)
{
    pthread_mutex_lock(&server_stat_mtx);

    if (server_stat.actual_max_files == 0)
    {
        pthread_mutex_unlock(&server_stat_mtx);
        return NULL;
    }

    FRecord *victims = (FRecord *)malloc(server_stat.actual_max_files * sizeof(FRecord));

    server_stat.actual_capacity += incoming_req_size;

    int n_to_eject = 0;

    pthread_mutex_lock(&storage_mutex);

    /* making a cycle to determine how many files need to be ejected
                       to make space for the new file */
    printf("### Calling LRU Check ###\n");
    printf("### server_stat.actual_capacity = %d ###\n", server_stat.actual_capacity);
    printf("### server_stat.actual_max_files = %d ###\n", server_stat.actual_max_files);
    while (((server_stat.actual_capacity > server_stat.capacity) && n_to_eject >= 0) || ((server_stat.actual_max_files > server_stat.max_files) && n_to_eject >= 0))
    {

        char *oldest_path = lru(storage_ht, incoming_path);

        printf("\n>>>victim selected: %s<<<\n", oldest_path);

        FRecord *cur_victim = (FRecord *)ht_get(storage_ht, oldest_path);

        memcpy(&(victims[n_to_eject]), cur_victim, sizeof(*cur_victim));

        server_stat.actual_capacity -= cur_victim->size;

        ht_delete(&storage_ht, oldest_path);

        if (oldest_path != NULL)
            free(oldest_path);

        n_to_eject++;
        server_stat.actual_max_files--;
    }

    pthread_mutex_unlock(&storage_mutex);
    pthread_mutex_unlock(&server_stat_mtx);

    *n_removed_files = n_to_eject;

    return victims;
}

char *lru(HashTable ht, char *incoming_path)
{
    if (storage_ht.size == 0)
        return NULL;

    char *oldest_path = (char *)malloc(sizeof(char) * MAX_PATHNAME);

    /* most recent time */
    time_t oldest;
    time(&oldest);

    /* for each row in ht */
    for (int row = 0; row < storage_ht.size; row++)
    {
        /* for each file record in this row */
        for (int index = 0; index < storage_ht.lists[row].length; index++)
        {
            if (storage_ht.lists[row].length > 0)
            {
                /* current record */
                FRecord *rec = (FRecord *)LL_get(storage_ht.lists[row], index);

                /* it could be the victim */
                if ((difftime(oldest, rec->last_edit) > 0) &&
                    (strcmp(rec->pathname, incoming_path) != 0))
                {
                    /* found a later time than oldest */
                    oldest = rec->last_edit;
                    strncpy(oldest_path, rec->pathname, MAX_PATHNAME);
                }
            }
        }
    }

    return oldest_path;
}

int log_init(char *log_pathname)
{
    printf("strlen of log pathname = %ld\n", strlen(log_pathname));

    return 0;
}

void clean_all(pthread_t **workers_tid, int *fd_socket, int *fd_client)
{
    destroyQueue(pending_requests);
    // free_table(storage_ht);

    free((*workers_tid));

    close((*fd_socket));
    close((*fd_client));
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
    if (cmd_code == 19)
        return "CLOSECONN";

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