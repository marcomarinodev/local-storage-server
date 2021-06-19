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
FILE *log_stream = NULL;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
    check_argc(argc);

    /* signal handling */
    int error;
    sigset_t mask;
    /* creating an empty signal mask */
    SYSCALL_EXIT("sigemptyset", error, sigemptyset(&mask), "sigemptyset error", "");

    /* Adding SIGINT, SIGQUIT, SIGHUP inside the mask */
    SYSCALL_EXIT("sigaddset", error, sigaddset(&mask, SIGINT), "sigaddset error", "");
    SYSCALL_EXIT("sigaddset", error, sigaddset(&mask, SIGQUIT), "sigaddset error", "");
    SYSCALL_EXIT("sigaddset", error, sigaddset(&mask, SIGHUP), "sigaddset error", "");

    /**
     * The signal mask is the set of signals whose
     * delivery is currently blocked for the caller.
     * SIGBLOCK: The set of blocked signals is the union
     * of the current set and the set argument.
    */
    SYSCALL_EXIT("pthread_sigmask", error, pthread_sigmask(SIG_BLOCK, &mask, NULL), "pthread_sigmask error", "");

    /* sigpipe handler using SIG_IGN (sig ignore) */
    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;

    /* set the action when SIGPIPE occurs */
    SYSCALL_EXIT("sigaction", error, sigaction(SIGPIPE, &s, NULL), "sigaction error", "");

    /* allocating the struct that contains the server setup */
    Setup *server_setup = (Setup *)safe_malloc(sizeof(Setup));

    /* calling the config parser */
    if (parse_config(server_setup, argv[1]) == -1)
    {
        printf("%d", errno);
        exit(EXIT_FAILURE);
    }

    /* server_setup init */
    server_stat.actual_capacity = 0;
    server_stat.actual_max_files = 0;
    server_stat.capacity = server_setup->max_storage;
    server_stat.max_files = server_setup->max_files_instorage;

    /* initializing log file */
    log_init(server_setup->log_path);

    /* log starter message (in mutual exclusion) */
    print_log("UNIX file-storage-server logger");
    print_log("parsed config file");

    /** creating the pending requests queue where all workers will access
      * to it in mutual exclusion.
    */
    pending_requests = createQueue(sizeof(ServerRequest));

    /* creating the actual storage (as hash table ds) */
    ht_create(&storage_ht, 1 + (server_stat.max_files / 2));

    /* array of workers tid ant their attributes */
    workers_tid = (pthread_t *)safe_malloc(sizeof(pthread_t) * server_setup->n_workers);
    attributes = (pthread_attr_t *)safe_malloc(sizeof(pthread_attr_t) * server_setup->n_workers);

    print_log("%d has been created", server_setup->n_workers);

    /* initialize the pipe in order to "unlock" the select
     * in case of SIGINT, SIGHUP, SIGQUIT and in case of 
     * re-listening to a certain client descriptor.
    */
    SYSCALL_EXIT("pipe", error, pipe(pfd), "pipe error", "");
    SYSCALL_EXIT("pipe", error, pipe(mwpipe), "pipe error", "");

    /* thread to handle signals */
    pthread_t sighandler_thread;

    /* creating the signal handler thread */
    safe_pcreate(&sighandler_thread, NULL, sigHandler, &mask);

    /* declaring the active_connections queue (as doubly linked list) */
    active_connections = NULL;

    /* main server function that manages connections (manager) */
    run_server(server_setup);

    /* all heap data structures are freed inside run_server function */
    /* join signal handler thread */
    printf("\n<<<Joining the signal thread>>>\n");
    safe_pjoin(sighandler_thread, NULL);

    /* closing log file */
    fclose(log_stream);

    return 0;
}

static void *sigHandler(void *arg)
{
    sigset_t *set = (sigset_t *)arg;
    int sig, res;

    sigwait(set, &sig);

    /* writing into the pipe which is the signal code */
    safe_plock(&spipe_mutex);

    /* delegating what to do to the manager */
    if ((res = writen(pfd[1], &sig, sizeof(int))) == -1)
    {
        perror("\n<<<FATAL ERROR 'writing into pipe error'>>>\n");
        exit(EXIT_FAILURE);
    }

    safe_punlock(&spipe_mutex);

    return (void *)NULL;
}

void spawn_thread(int index)
{
    int error;
    sigset_t mask, oldmask;

    SYSCALL_EXIT("sigemptyset", error, sigemptyset(&mask), "sigemptyset error", "");
    SYSCALL_EXIT("sigaddset", error, sigaddset(&mask, SIGINT), "sigaddset error", "");
    SYSCALL_EXIT("sigaddset", error, sigaddset(&mask, SIGQUIT), "sigaddset error", "");
    SYSCALL_EXIT("sigaddset", error, sigaddset(&mask, SIGHUP), "sigaddset error", "");

    SYSCALL_EXIT("pthread_sigmask", error, pthread_sigmask(SIG_BLOCK, &mask, &oldmask), "pthread_sigmask error", "");

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

    /* I didn't use safe_pcreate because I need to destroy attributes previously initializated */
    if (pthread_create(&workers_tid[index], &attributes[index], &worker_func, NULL) != 0)
    {
        fprintf(stderr, "pthread_create FALLITA");
        safe_pattr_destroy(&attributes[index]);
        close(fd_socket);
        return;
    }

    SYSCALL_EXIT("pthread_sigmask", error, pthread_sigmask(SIG_BLOCK, &oldmask, NULL), "pthread_sigmask error", "");
}

static void run_server(Setup *server_setup)
{
    struct sockaddr_un sa;
    int error;
    int fd_client;
    int fd_num = 0;
    int fd, sig;
    ServerRequest new_request;
    fd_set rdset;

    /* socket initialization */
    unlink(server_setup->server_socket_pathname);
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, server_setup->server_socket_pathname, strlen(server_setup->server_socket_pathname) + 1);

    SYSCALL_EXIT("socket", fd_socket, socket(AF_UNIX, SOCK_STREAM, 0), "Socket init error", "");
    SYSCALL_EXIT("binding", error, bind(fd_socket, (struct sockaddr *)&sa, sizeof(sa)), "bind error", "");
    SYSCALL_EXIT("listen", error, listen(fd_socket, SOMAXCONN), "listen error", "");

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

    print_log("Server is listening to incoming requests. The fd_socket is %d", fd_socket);

    /* in order to manage SIGHUP, i cannot get out from select, otherwise i'm not going be able
     * to listen to new requests from pending connected clients */

    /* SIGINT/SIGQUIT is simple, we only have to close all of connections inside the LList,
     * resume workers until they exit and free their tcbs with pthread_join, and of course
     * clean all of the objects inside the heap
    */
    while (ending_all != TRUE)
    {
        safe_plock(&ac_mutex);
        rdset = active_set; /* preparo maschera per select */
        safe_punlock(&ac_mutex);

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

                        safe_plock(&ac_mutex);

                        FD_SET(fd_client, &active_set);

                        /* append the new connection to active list */
                        d_append(&active_connections, fd_client);

                        dim++;

                        printf("\n %%%% NEW_CONN occured: ACTIVE CONNECTIONS with dim = %d ", dim);
                        d_print(active_connections);

                        safe_punlock(&ac_mutex);

                        if (fd_client > fd_num)
                            fd_num = fd_client;

                        print_log("client %d is connected", fd_client);
                    }
                    else
                    { /* sock I/0 ready */

                        safe_plock(&spipe_mutex);

                        if (fd == pfd[0])
                        {
                            int res;
                            /* reading signal code from pipe */
                            if ((res = readn(pfd[0], &sig, sizeof(int))) == -1)
                            {
                                print_log("(FATAL ERROR) pipe reading");
                                exit(EXIT_FAILURE);
                            }

                            FD_CLR(pfd[0], &active_set);
                            safe_punlock(&spipe_mutex);

                            /* case if signal is SIGINT or SIGQUIT */
                            /* what we have to do is simply close all active connections
                             * and terminate the workers properly
                            */
                            if (sig == SIGINT || sig == SIGQUIT)
                            {
                                ending_all = TRUE;
                                /* taking the lock of the requests before waking up asleep workers */
                                safe_plock(&pending_requests_mutex);

                                is_sigquit = TRUE;

                                print_log("(SIGINT/SIGQUIT) waking up asleep workers");

                                safe_cbroadcast(&pending_requests_cond);

                                safe_punlock(&pending_requests_mutex);
                                break;
                            }
                            else if (sig == SIGHUP)
                            {
                                /* if the signal is SIGHUP I have to close the server socket
                                 * and clear its bit inside the bitmap. We also need the mutex
                                 * of active connections d_list in order to check if it is free.
                                */
                                safe_plock(&ac_mutex);

                                is_sighup = TRUE;

                                print_log("(SIGHUP)");

                                close(fd_socket);
                                FD_CLR(fd_socket, &active_set);

                                printf("\n %%%% SIGHUP occured: ACTIVE CONNECTIONS with dim = %d ", dim);
                                d_print(active_connections);

                                /* if there are some active connections, then the server must complete their
                                 * requests, so we do another select loop,
                                */
                                if (dim == 0)
                                {
                                    /* do what we did for SIGQUIT/SIGINT */
                                    if (active_connections != NULL)
                                        printf("FAILED INVARIANT => %d\n", active_connections->data);

                                    /* taking the lock of the requests before waking up asleep workers */
                                    safe_plock(&pending_requests_mutex);

                                    is_sigquit = TRUE;

                                    print_log("(SIGHUP) There are no more connections. Waking up asleep workers");

                                    safe_cbroadcast(&pending_requests_cond);

                                    safe_punlock(&pending_requests_mutex);
                                    safe_punlock(&ac_mutex);

                                    ending_all = TRUE;

                                    break;
                                }

                                safe_punlock(&ac_mutex);
                            }
                        }
                        else
                        {
                            safe_punlock(&spipe_mutex);

                            /* master worker pipe checking */
                            safe_plock(&mwpipe_mutex);

                            /* pipe reading (re-listen to a client) */
                            if (fd == mwpipe[0])
                            {
                                int res, new_desc;
                                if ((res = readn(mwpipe[0], &new_desc, sizeof(int))) == -1)
                                {
                                    print_log("(FATAL ERROR) mwpipe error");
                                    exit(EXIT_FAILURE);
                                }

                                /* if a worker needs to write on pipe, it needs to find this variable at TRUE */
                                print_log("Pipe is available for other workers");
                                can_pipe = 1;

                                /* re-listen to the socket */
                                FD_SET(new_desc, &active_set);

                                /* update maximum fd */
                                if (new_desc > fd_num)
                                    fd_num = new_desc;

                                safe_csignal(&workers_done);
                                safe_punlock(&mwpipe_mutex);
                            }
                            else
                            { /* normal request */
                                safe_punlock(&mwpipe_mutex);
                                memset(&new_request, 0, sizeof(ServerRequest));

                                int n_read = readn(fd, &new_request, sizeof(ServerRequest));

                                /* EOF in fd */
                                if (n_read == 0)
                                {
                                    close(fd);

                                    safe_plock(&ac_mutex);
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

                                        safe_plock(&pending_requests_mutex);

                                        is_sigquit = TRUE;

                                        safe_cbroadcast(&pending_requests_cond);
                                        safe_punlock(&pending_requests_mutex);

                                        safe_punlock(&ac_mutex);

                                        break;
                                    }

                                    print_log("client %d is disconnected", fd);

                                    safe_punlock(&ac_mutex);
                                }
                                else
                                {
                                    new_request.fd_cleint = fd;

                                    /* suspending manager to listen this descriptor */
                                    safe_plock(&ac_mutex);
                                    FD_CLR(fd, &active_set);
                                    safe_punlock(&ac_mutex);

                                    print_log("\nIncoming request:\n- calling client pid: %d\n- cmd: %s\n- path: %s",
                                              new_request.calling_client,
                                              cmd_type_to_string(new_request.cmd_type),
                                              new_request.pathname);

                                    // metti in coda
                                    safe_plock(&pending_requests_mutex);

                                    enqueue(pending_requests, &new_request);
                                    safe_csignal(&pending_requests_cond);

                                    safe_punlock(&pending_requests_mutex);
                                }
                            }
                        }
                    }
                }
            }

            printf("\n");
        }
    }

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

    print_log("\nStorage Status\n- capacity = %d out of %d\n- n.files = %d out of %d",
              server_stat.actual_capacity, server_stat.capacity,
              server_stat.actual_max_files, server_stat.max_files);

    printf("\n\t(***) STORAGE (***)\n");
    printf("\tcapacity = %d out of %d\n", server_stat.actual_capacity, server_stat.capacity);
    printf("\tfiles     = %d out of %d\n", server_stat.actual_max_files, server_stat.max_files);
    ht_print(storage_ht);
    printf("\n\t(*****************)\n");

    print_log("broadcasting to workers");
    safe_cbroadcast(&pending_requests_cond);

    for (int i = 0; i < server_setup->n_workers; i++)
    {
        print_log("\n<<<Joining %d worker thread>>>\n", i);
        printf("\n<<<Joining %d worker thread>>>\n", i);
        safe_pjoin(workers_tid[i], NULL);
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

    print_log("thread %ld attivo", pthread_self());
    printf("--- thread %ld attivo ---\n", pthread_self());

    while (TRUE)
    {
        safe_plock(&pending_requests_mutex);

        if (isEmpty(pending_requests) == 1)
            safe_cwait(&pending_requests_cond, &pending_requests_mutex);

        if (is_sigquit == TRUE)
        {
            printf("\n<<<closing worker>>>\n");
            safe_punlock(&pending_requests_mutex);
            pthread_exit(NULL);
        }

        dequeue(pending_requests, &incoming_request);

        safe_punlock(&pending_requests_mutex);

        switch (incoming_request.cmd_type)
        {
        case OPEN_FILE_REQ:
        {
            Response response;
            memset(&response, 0, sizeof(Response));

            print_log("Trying to open with flag %d: %s", incoming_request.flags, incoming_request.pathname);

            if (incoming_request.flags == O_CREATE)
            {
                printf("OCREATE\n");
                safe_plock(&storage_mutex);

                /* O_CREATE - file already exists in storage */
                if (ht_exists(storage_ht, incoming_request.pathname))
                {
                    safe_punlock(&storage_mutex);

                    /* returns error code */
                    print_log("%s already exists", incoming_request.pathname);
                    response.code = FILE_ALREADY_EXISTS;
                }
                else /* O_CREATE - file does not exists */
                {
                    if (incoming_request.size <= server_stat.capacity)
                    { /* case when storage can contain the file sent in request */

                        FRecord *new = (FRecord *)safe_malloc(sizeof(FRecord));
                        memset(new, 0, sizeof(FRecord));

                        char *key = (char *)safe_malloc(MAX_PATHNAME * sizeof(char));

                        memset(key, 0, MAX_PATHNAME);

                        strncpy(new->pathname, incoming_request.pathname, MAX_PATHNAME);
                        strncpy(key, new->pathname, MAX_PATHNAME);

                        /* lock init */
                        safe_pmutex_init(&(new->lock), NULL);
                        /* file is new and open */
                        new->is_open = TRUE;
                        new->is_new = TRUE;
                        /* set dimension, last client (this) and last operation (this) */
                        new->size = incoming_request.size;
                        new->last_client = incoming_request.calling_client;
                        new->last_op = O_CREATE_SUCCESS;

                        ht_insert(&storage_ht, new, key);

                        print_log("%s created and opened with success", incoming_request.pathname);

                        response.code = O_CREATE_SUCCESS;

                        safe_plock(&server_stat_mtx);

                        server_stat.actual_max_files++;

                        safe_punlock(&server_stat_mtx);

                        safe_punlock(&storage_mutex);
                    }
                    else
                    {
                        safe_punlock(&storage_mutex);
                        response.code = STRG_OVERFLOW;
                    }
                }
            }
            else
            {
                /* O_CREATE is not specified */
                safe_plock(&storage_mutex);

                /* file exists in storage, we need to open it */
                if (ht_exists(storage_ht, incoming_request.pathname))
                {
                    FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);
                    safe_punlock(&storage_mutex);

                    safe_plock(&rec->lock);
                    rec->is_open = TRUE;
                    safe_punlock(&rec->lock);

                    print_log("%s opened with success", incoming_request.pathname);

                    response.code = OPEN_SUCCESS;
                }
                else
                {
                    safe_punlock(&storage_mutex);

                    response.code = FAILED_FILE_SEARCH;
                }
            }

            printf("writing to fd: %d\n", incoming_request.fd_cleint);
            writen(incoming_request.fd_cleint, &response, sizeof(response));
        }
        break;

        case APPEND_FILE_REQ:
        {
            print_log("attempting to appendFile to %s", incoming_request.pathname);

            /* the append operation is atomic, so we need to take the lock of the server */
            safe_plock(&storage_mutex);

            Response response;
            memset(&response, 0, sizeof(Response));

            /* searching for the file */
            if (ht_exists(storage_ht, incoming_request.pathname))
            {
                /* if it exists then get it */
                FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);
                safe_punlock(&storage_mutex);

                if (incoming_request.size > server_stat.capacity)
                    response.code = STRG_OVERFLOW;
                else
                {
                    safe_plock(&rec->lock);
                    /* in order to deselect rec from the LRU search ==> rec->is_new = TRUE */
                    rec->is_new = TRUE;

                    /* lru check */
                    int n_to_eject;
                    FRecord *files_to_send = select_lru_victims(incoming_request.size, incoming_request.pathname, &n_to_eject);

                    response.code = n_to_eject;

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

                        strncpy(file_response.path, files_to_send[i].pathname, strlen(files_to_send[i].pathname) + 1);
                        memcpy(file_response.content, files_to_send[i].content, files_to_send[i].size);

                        writen(incoming_request.fd_cleint, &file_response, sizeof(file_response));

                        if (files_to_send[i].content != NULL)
                            free(files_to_send[i].content);
                    }

                    if (files_to_send != NULL)
                        free(files_to_send);

                    /* Having space to append... */
                    time_t now = time(NULL);
                    size_t old_size = rec->size;

                    rec->is_new = FALSE;
                    rec->last_op = APPEND_FILE_SUCCESS;
                    rec->last_edit = now;
                    rec->size += incoming_request.size;

                    if (rec->content == NULL)
                    {
                        rec->content = safe_malloc(1 + (sizeof(char) * incoming_request.size));
                        memcpy(rec->content, incoming_request.content, incoming_request.size);
                    }
                    else
                    {
                        print_log("appending other %d space to %s", incoming_request.size, rec->pathname);

                        char *new_content = conc(old_size, rec->content, incoming_request.content);

                        free(rec->content);

                        rec->content = safe_malloc(1 + (sizeof(char) * (incoming_request.size + old_size)));
                        memcpy(rec->content, new_content, old_size + incoming_request.size);

                        free(new_content);
                    }

                    safe_punlock(&rec->lock);

                    safe_plock(&server_stat_mtx);
                    server_stat.actual_capacity += incoming_request.size;
                    safe_punlock(&server_stat_mtx);

                    response.code = APPEND_FILE_SUCCESS;
                }
            }

            writen(incoming_request.fd_cleint, &response, sizeof(response));
            safe_punlock(&storage_mutex);
        }
        break;

        case WRITE_FILE_REQ:
        {
            print_log("attempting to writeFile to %s", incoming_request.pathname);
            Response response;
            memset(&response, 0, sizeof(Response));

            safe_plock(&storage_mutex);

            time_t now = time(0);

            if (ht_exists(storage_ht, incoming_request.pathname))
            {
                FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);

                safe_punlock(&storage_mutex);

                print_log("file opened with O_CREATE exists and its path is: %s\n", rec->pathname);

                /* locking the file to perform this operation */
                safe_plock(&rec->lock);

                /* if the file exists and its last operation was openFile with O_CREATE flag then we can write on it */
                if (rec->is_open == TRUE && rec->last_op == O_CREATE_SUCCESS && rec->last_client == incoming_request.calling_client)
                {
                    print_log("Writing on %s opened with O_CREATE\n", rec->pathname);
                    /* use response.code as n_to_eject */

                    /* lru check */
                    int n_to_eject;

                    /* it deletes lru victims and store them into an array called files_to_send */
                    FRecord *files_to_send = select_lru_victims(incoming_request.size, incoming_request.pathname, &n_to_eject);

                    response.code = n_to_eject;

                    writen(incoming_request.fd_cleint, &response, sizeof(response));

                    /* now the server knows which files to send
                     * so at the end of a single cycle a writen will be made in order to
                     * send the ejected file
                     *
                     * cycling files_to_send array 
                    */
                    for (int i = 0; i < n_to_eject; i++)
                    {
                        Response file_response;
                        memset(&file_response, 0, sizeof(file_response));

                        file_response.content_size = files_to_send[i].size;

                        strncpy(file_response.path, files_to_send[i].pathname, strlen(files_to_send[i].pathname) + 1);
                        memcpy(file_response.content, files_to_send[i].content, files_to_send[i].size);

                        printf("sending file...\n");
                        writen(incoming_request.fd_cleint, &file_response, sizeof(file_response));

                        if (files_to_send[i].content != NULL)
                            free(files_to_send[i].content);
                    }

                    if (files_to_send != NULL)
                        free(files_to_send);

                    /* write to the file */
                    /* updating metadata */
                    rec->last_edit = now;
                    rec->last_op = WRITE_SUCCESS;
                    rec->is_new = FALSE;

                    /* init space for content */
                    if (rec->content == NULL)
                        rec->content = safe_malloc(1 + (sizeof(char) * incoming_request.size));

                    memset(rec->content, 0, 1 + incoming_request.size);
                    memcpy(rec->content, incoming_request.content, incoming_request.size);

                    safe_punlock(&(rec->lock));

                    print_log("%s was succesfully written", rec->pathname);

                    response.code = WRITE_SUCCESS;
                }
                else
                {
                    safe_punlock(&(rec->lock));
                    /* before you do a write operation, you need to open the file */
                    printf("\n<<<The client must open the file using O_CREATE flags>>>\n");
                    response.code = WRITE_FAILED;
                }
            }
            else /* the file does not exists, so I am going to return a write file error */
            {
                safe_punlock(&storage_mutex);
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
            print_log("attempting to read %s", incoming_request.pathname);

            Response response;
            memset(&response, 0, sizeof(Response));

            safe_plock(&storage_mutex);

            if (ht_exists(storage_ht, incoming_request.pathname))
            {
                FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);
                safe_punlock(&storage_mutex);

                safe_plock(&rec->lock);

                /* sending the size of the content, in order to read exactly that size */
                if (rec->content != NULL && rec->is_open == TRUE)
                {
                    response.content_size = rec->size;

                    /* put path and content in the response struct */
                    strncpy(response.path, incoming_request.pathname, strlen(incoming_request.pathname) + 1);
                    memcpy(response.content, rec->content, rec->size);

                    response.code = READ_SUCCESS;
                }
                else
                {
                    /* two kinds of problem: content is null or file is not opened */
                    response.code = FILE_NOT_OPENED;
                }

                safe_punlock(&rec->lock);
            }
            else
            {
                safe_punlock(&storage_mutex);
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

            safe_plock(&storage_mutex);

            /* if the clients wants to read more than max_files files,
             * or the flag is not specified, then the server will
             * send all files in storage.
            */

            safe_plock(&server_stat_mtx);

            if (n < 0 || n > server_stat.actual_max_files)
            {
                response.code = server_stat.actual_max_files;
            }
            else /* otherwise server will send exactly n files */
                response.code = n;

            safe_punlock(&server_stat_mtx);

            /* telling to the client that it will have to receive n files */
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
                            response.content_size = rec->size;

                            strncpy(response.path, rec->pathname, strlen(rec->pathname) + 1);
                            memcpy(response.content, rec->content, rec->size);

                            response.code = READ_SUCCESS;
                            writen(incoming_request.fd_cleint, &response, sizeof(response));

                            print_log("sent file with:\n- path = %s\n- size = %ld", rec->pathname, rec->size);

                            k++;
                        }
                    }
                    else
                        break;
                }

                if (k >= n_to_read)
                    break;
            }

            print_log("%d files successfully sent to %d", k, incoming_request.fd_cleint);
            safe_punlock(&storage_mutex);
        }

        break;

        case CLOSE_FILE_REQ:
        {
            print_log("attempting to close %s file", incoming_request.pathname);
            Response response;
            memset(&response, 0, sizeof(response));

            safe_plock(&storage_mutex);

            if (ht_exists(storage_ht, incoming_request.pathname))
            {
                FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);
                safe_punlock(&storage_mutex);

                safe_plock(&(rec->lock));
                if (rec->is_open == TRUE)
                {

                    /* if the client that made the closing request is the last client */
                    if (rec->last_client == incoming_request.calling_client)
                    {
                        rec->is_open = FALSE;
                        rec->is_new = FALSE;
                        response.code = CLOSE_FILE_SUCCESS;
                        print_log("%s closed with success", incoming_request.pathname);
                    }
                    else
                    {
                        /* if is not the client who made the lock ==> illegal, response code is FILE_IS_LOCKED */
                        response.code = NOT_AUTH;
                        print_log("a client other than the one who opened the file, tried to close the file");
                    }
                }
                else
                {
                    /* the file is not open, so is already closed */
                    response.code = IS_ALREADY_CLOSED;
                }
                safe_punlock(&(rec->lock));
            }
            else
            {
                safe_punlock(&storage_mutex);
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
        safe_plock(&mwpipe_mutex);

        while (can_pipe == 0)
            safe_cwait(&workers_done, &mwpipe_mutex);

        can_pipe = 0;

        int res;

        if ((res = writen(mwpipe[1], &(incoming_request.fd_cleint), sizeof(int))) == -1)
        {
            perror("Writing on pipe error");
            exit(EXIT_FAILURE);
        }

        safe_punlock(&mwpipe_mutex);

        // print_log("wrote on pipe\n");
    }

    pthread_exit((void *)NULL);
}

char *conc(size_t size1, char const *str1, char const *str2)
{
    size_t const l2 = strlen(str2);

    char *result = safe_malloc(size1 + l2 + 1);
    if (!result)
        return result;
    memcpy(result, str1, size1);
    memcpy(result + size1, str2, l2 + 1);
    return result;
}

FRecord *select_lru_victims(size_t incoming_req_size, char *incoming_path, int *n_removed_files)
{
    safe_plock(&server_stat_mtx);

    if (server_stat.actual_max_files == 0)
    {
        safe_punlock(&server_stat_mtx);
        return NULL;
    }

    FRecord *victims = (FRecord *)safe_malloc(server_stat.actual_max_files * sizeof(FRecord));

    server_stat.actual_capacity += incoming_req_size;

    int n_to_eject = 0;

    safe_plock(&storage_mutex);

    /* making a cycle to determine how many files need to be ejected
                       to make space for the new file */
    print_log("\nLRU check\n- capacity stat = %d\n- n.files stat = %d\n",
              server_stat.actual_capacity, server_stat.actual_max_files);

    while (((server_stat.actual_capacity > server_stat.capacity) && n_to_eject >= 0) || ((server_stat.actual_max_files > server_stat.max_files) && n_to_eject >= 0))
    {
        print_log("calling LRU function");
        char *oldest_path = lru(storage_ht, incoming_path);

        if (oldest_path == NULL)
        {
            print_log("LRU did not find a file to delete");
            return NULL;
        }

        print_log("victim selected and deleted is %s", oldest_path);

        FRecord *cur_victim = (FRecord *)ht_get(storage_ht, oldest_path);

        memset(&(victims[n_to_eject]), 0, sizeof(FRecord));
        memcpy(&(victims[n_to_eject]), cur_victim, sizeof(FRecord));

        server_stat.actual_capacity -= cur_victim->size;

        ht_delete(&storage_ht, oldest_path);

        if (oldest_path != NULL)
            free(oldest_path);

        n_to_eject++;
        server_stat.actual_max_files--;
    }

    safe_punlock(&storage_mutex);
    safe_punlock(&server_stat_mtx);

    print_log("LRU has removed %d files from storage", n_to_eject);

    *n_removed_files = n_to_eject;

    return victims;
}

char *lru(HashTable ht, char *incoming_path)
{
    int found = -1;

    if (storage_ht.size == 0)
        return NULL;

    char *oldest_path = (char *)safe_malloc(sizeof(char) * MAX_PATHNAME);

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

                char time_buff[30];
                struct tm *info;
                info = localtime(&rec->last_edit);

                strftime(time_buff, sizeof(time_buff), "%b %d %H:%M:%S", info);

                print_log("\n%s\n- is_new: %d\n- is_open: %d\n- last_op: %d\n- last_client: %d\n- last_edit: %s",
                          rec->pathname, rec->is_new, rec->is_open, rec->last_op, rec->last_client, time_buff);

                /* it could be the victim */
                if ((rec->last_edit <= oldest) &&
                    (strcmp(rec->pathname, incoming_path) != 0))
                {
                    /* found a later time than oldest */
                    found = 1;
                    oldest = rec->last_edit;
                    strncpy(oldest_path, rec->pathname, MAX_PATHNAME);
                }
            }
        }
    }

    if (found == 1)
        return oldest_path;
    else
    {
        free(oldest_path);
        return NULL;
    }
}

int log_init(char *config_logpath)
{
    printf("config log path: %s\n", config_logpath);
    char logs_folder_pathname[MAX_PATHNAME];
    char logs_filename[MAX_LOGFILENAME];

    int error;

    /* naming the log file as the date time of the log */
    time_t rawtime;
    struct tm *info;

    time(&rawtime);
    info = localtime(&rawtime);

    strftime(logs_filename, MAX_LOGFILENAME, "%d-%b-%Y.txt", info);

    printf("log filename: %s\n", logs_filename);

    /* searching for logs folder */
    DIR *logs_folder = opendir(config_logpath);

    if (logs_folder == NULL)
    {
        printf("creating ServerLogs folder\n");
        /* create the directory if it does not exist */
        error = mkdir(config_logpath, 0777);

        if (error != 0)
        {
            perror("cannot create logs folder");
            exit(EXIT_FAILURE);
        }
        else
        {
            sprintf(logs_folder_pathname, "%s/%s", config_logpath, logs_filename);
        }
    }
    else
    {
        SYSCALL_EXIT("closedir", error, closedir(logs_folder), "cannot close logs directory", "");
        sprintf(logs_folder_pathname, "%s/%s", config_logpath, logs_filename);
    }

    printf("log file is available on %s\n", logs_folder_pathname);

    log_stream = fopen(logs_folder_pathname, "w");

    if (!log_stream)
        printf("cannot create log (errno = %d)\n", errno);

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

void print_log(const char *format, ...)
{
    char buffer[256];
    char time_string[MAX_TIMESTRING];
    va_list args;
    time_t rawtime;
    struct tm *info;

    time(&rawtime);
    info = localtime(&rawtime);

    va_start(args, format);
    vsnprintf(buffer, 256, format, args);
    va_end(args);

    safe_plock(&log_mutex);

    if (log_stream)
    {
        /* timestamp to string */
        strftime(time_string, MAX_TIMESTRING, "%I:%M:%S", info);

        /* write to log */
        fprintf(log_stream, "%s fss ==> %s\n\n", time_string, buffer);

        fflush(log_stream);
    }

    safe_punlock(&log_mutex);
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