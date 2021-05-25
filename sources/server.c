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
static pthread_mutex_t server_socket_mutex = PTHREAD_MUTEX_INITIALIZER;

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
static pthread_mutex_t pfd_mutex = PTHREAD_MUTEX_INITIALIZER;

/* queue of client_fd elements */
static LList active_connections;
static pthread_mutex_t ac_mutex = PTHREAD_MUTEX_INITIALIZER;

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

    Server_setup *server_setup = (Server_setup *)malloc(sizeof(Server_setup));

    parse_config(argv[1], &(server_setup->config_info), CONFIG_ROWS);
    save_setup(&server_setup);

    pending_requests = createQueue(sizeof(ServerRequest));
    LL_init(&active_connections, print_conn);

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
            close(pfd[0]);
            return NULL;
            break;
        case SIGQUIT:
            ending_all = 1;

            close(pfd[1]);
            return NULL;
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

static void run_server(Server_setup *server_setup)
{
    struct sockaddr_un sa;

    int fd_client;
    int fd_num = 0;
    int fd, _sig;
    ServerRequest new_request;
    fd_set rdset;

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, server_setup->config_info[0]);

    Pthread_mutex_lock_(&server_socket_mutex, "server socket");

    fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    // unlink(sa.sun_path);
    bind(fd_socket, (struct sockaddr *)&sa, sizeof(sa));
    listen(fd_socket, SOMAXCONN);

    Pthread_mutex_lock_(&ac_mutex, "active connections");
    FD_ZERO(&active_set);
    FD_SET(fd_socket, &active_set);
    Pthread_mutex_unlock_(&ac_mutex, "active connections");

    /* max active fd is fd_num */
    if (fd_socket > fd_num)
        fd_num = fd_socket;

    Pthread_mutex_unlock_(&server_socket_mutex, "server socket");

    Pthread_mutex_lock_(&mwpipe_mutex, "mw pipe");
    Pthread_mutex_lock_(&ac_mutex, "active connections");
    /* register the read endpoint descriptor of the pipe */
    FD_SET(mwpipe[0], &active_set);
    Pthread_mutex_unlock_(&ac_mutex, "active connections");
    if (mwpipe[0] > fd_num)
        fd_num = mwpipe[0];
    Pthread_mutex_unlock_(&mwpipe_mutex, "mw pipe");

    Pthread_mutex_lock_(&pfd_mutex, "pfd pipe");
    Pthread_mutex_lock_(&ac_mutex, "active connections");
    /* register the read endpoint descriptor of the pipe */
    FD_SET(pfd[0], &active_set);
    Pthread_mutex_unlock_(&ac_mutex, "active connections");
    /* update fd_num */
    if (pfd[0] > fd_num)
        fd_num = pfd[0];
    Pthread_mutex_unlock_(&pfd_mutex, "pfd pipe");

    for (int i = 0; i < server_setup->n_workers; i++)
        spawn_thread(i);

    printf("\n<<<Listen to requests, fd_socket = %d>>>\n", fd_socket);

    /* in order to manage SIGHUP, i cannot get out from select, otherwise i'm not going be able
     * to listen to new requests from pending connected clients */

    /* SIGINT/SIGQUIT is simple, we only have to close all of connections inside the LList,
     * resume workers until they exit and free their tcbs with pthread_join, and of course
     * clean all of the objects inside the heap
    */
    while (!ending_all)
    {
        Pthread_mutex_lock_(&ac_mutex, "active connections");
        rdset = active_set; /* preparo maschera per select */
        Pthread_mutex_unlock_(&ac_mutex, "active connections");

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        if (select(fd_num + 1, &rdset, NULL, NULL, &timeout) == -1)
        { /* gest errore */
            perror("select");
            exit(EXIT_FAILURE);
        }
        else
        { /* select OK */
            for (fd = 0; fd <= fd_num; fd++)
            {
                if (FD_ISSET(fd, &rdset))
                    printf("\n=======> %d is set\n", fd);

                
                /* descriptor is ready */
                if (FD_ISSET(fd, &rdset))
                {
                    printf("\nsomething is set\n");
                    /* listen socket ready ==> accept is not blocking */
                    if (fd == fd_socket)
                    {
                        _conn *new_con = (_conn *)malloc(sizeof(_conn));
                        memset(new_con, 0, sizeof(*new_con));

                        new_con->fd = accept(fd_socket, NULL, 0);

                        /* new connection => push to queue */
                        Pthread_mutex_lock_(&ac_mutex, "active connections");

                        FD_SET(new_con->fd, &active_set);

                        if (new_con->fd > fd_num)
                            fd_num = new_con->fd;

                        char *cl_key = (char *)malloc(sizeof(char) * 4);
                        memset(cl_key, 0, 4);

                        sprintf(cl_key, "%d", new_con->fd);

                        LL_enqueue(&active_connections, (void *)new_con, cl_key);
                        printf("\n((( %d is connected )))\n", new_con->fd);

                        Pthread_mutex_unlock_(&ac_mutex, "active connections");
                    }
                    else
                    { /* sock I/0 ready */
                        /* master worker pipe checking */
                        Pthread_mutex_lock_(&mwpipe_mutex, "mw pipe");

                        /* pipe reading (re-listen to a client) */
                        if (fd == mwpipe[0])
                        {
                            int res, new_desc;
                            if ((res = readn(mwpipe[0], &new_desc, sizeof(int))) == -1)
                            {
                                perror("Read mw pipe error");
                                exit(EXIT_FAILURE);
                            }

                            printf("<<<%d needs to be listened again>>>\n", new_desc);

                            /* if a worker needs to write on pipe, it needs to find this variable at TRUE */
                            can_pipe = 1;

                            /* re-listen to the socket */
                            Pthread_mutex_lock_(&ac_mutex, "active connections");
                            FD_SET(new_desc, &active_set);

                            /* update maximum fd */
                            if (new_desc > fd_num)
                                fd_num = new_desc;

                            Pthread_mutex_unlock_(&ac_mutex, "active connections");

                            Pthread_cond_signal(&workers_done);

                            Pthread_mutex_unlock_(&mwpipe_mutex, "mw pipe");
                        }
                        else
                        { /* normal request */
                            Pthread_mutex_unlock_(&mwpipe_mutex, "mw pipe");

                            memset(&new_request, 0, sizeof(ServerRequest));

                            int n_read = readn(fd, &new_request, sizeof(ServerRequest));

                            /* EOF in fd */
                            if (n_read == 0)
                            {
                                close(fd);

                                Pthread_mutex_lock_(&ac_mutex, "active connections");
                                FD_CLR(fd, &active_set);

                                fd_num = update_fds(active_set, fd_num);

                                /* we need to delete this fd from connections list */

                                char key[4];

                                sprintf(key, "%d", fd);

                                printf("\n---> closing connection %s\n", key);
                                LL_remove_by_key(&active_connections, key);

                                Pthread_mutex_unlock_(&ac_mutex, "active connections");
                            }
                            else
                            {
                                new_request.fd_cleint = fd;

                                /* suspending manager to listen this descriptor */
                                Pthread_mutex_lock_(&ac_mutex, "active connections");
                                FD_CLR(fd, &active_set);

                                fd_num = update_fds(active_set, fd_num);
                                Pthread_mutex_unlock_(&ac_mutex, "active connections");

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
    }

    printf("\n<<<SIGNAL RECEIVED>>>\n");

    LL_print(active_connections, "--- ACTIVE CONNECTIONS BEFORE CLOSING ALL CONNECTIONS ---");

    /* close all connections inside the active_connections LList */
    Node *curr = active_connections.head;

    if (curr != NULL)
    {
        while (curr->next != NULL)
        {
            int curr_fd = *(int *)curr->data;
            close(fd);
        }
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

    LL_free(active_connections, NULL);

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
    ht_free(storage_ht);
}

int update_fds(fd_set set, int fd_num)
{
    int i, max = 0;

    for (i = 0; i < fd_num; i++)
    {
        if (FD_ISSET(i, &set))
        {
            if (i > max)
                max = i;
        }
    }
    return max;
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

                        int n_to_eject = select_lru_victims(incoming_request.size, incoming_request.pathname);
                        response.code = n_to_eject;
                        writen(incoming_request.fd_cleint, &response, sizeof(response));

                        /* sending (if exist) file deleted by lru algo */
                        /* foreach row in ht */
                        for (int row = 0; row < storage_ht.size; row++)
                        {

                            if (n_to_eject == 0)
                            {
                                printf("\n<<<no more files to eject>>>\n");
                                break;
                            }

                            /* for each file record in this row */
                            for (int index = 0; index < storage_ht.lists[row].length; index++)
                            {
                                /* current record */
                                FRecord *r = (FRecord *)LL_get(storage_ht.lists[row], index);

                                Response file_response;
                                memset(&file_response, 0, sizeof(file_response));

                                if (r != NULL)
                                {
                                    if (r->is_locked == FALSE && r->is_victim == TRUE)
                                    {
                                        printf(">>>I found what I have to delete<<<\n");

                                        file_response.content_size = r->size;

                                        printf("sizeof(rec->content) = %ld\n", rec->size);

                                        strncpy(file_response.path, r->pathname, strlen(r->pathname) + 1);
                                        memcpy(file_response.content, r->content, r->size);

                                        printf("sending file...\n");
                                        writen(incoming_request.fd_cleint, &file_response, sizeof(file_response));

                                        server_stat.actual_capacity -= r->size;
                                        server_stat.actual_max_files--;
                                        ht_delete(&storage_ht, r->pathname);

                                        n_to_eject--;
                                    }
                                }
                            }
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

            Pthread_mutex_lock(&storage_mutex, pthread_self(), "storage");

            time_t now = time(0);

            if (ht_exists(storage_ht, incoming_request.pathname))
            {
                FRecord *rec = (FRecord *)ht_get(storage_ht, incoming_request.pathname);
                if (rec->is_open == TRUE && rec->is_locked == TRUE)
                {
                    /* use response.code as n_to_eject */

                    /* lru check */
                    int n_to_eject = select_lru_victims(incoming_request.size, incoming_request.pathname);
                    response.code = n_to_eject;
                    writen(incoming_request.fd_cleint, &response, sizeof(response));

                    /* now the server knows how many files need to be ejected */
                    /* so at the end of a single cycle a writen will be made in order to
                     * send the ejected file
                    */

                    /* foreach row in ht */
                    for (int row = 0; row < storage_ht.size; row++)
                    {

                        if (n_to_eject == 0)
                        {
                            printf("\nnon ho file da espellere\n");
                            break;
                        }

                        /* for each file record in this row */
                        for (int index = 0; index < storage_ht.lists[row].length; index++)
                        {
                            /* current record */
                            FRecord *r = (FRecord *)LL_get(storage_ht.lists[row], index);

                            Response file_response;
                            memset(&file_response, 0, sizeof(file_response));

                            if (r != NULL)
                            {
                                if (r->is_locked == FALSE && r->is_victim == TRUE)
                                {
                                    printf(">>>I found what I have to delete<<<\n");

                                    file_response.content_size = r->size;

                                    printf("sizeof(rec->content) = %ld\n", rec->size);

                                    strncpy(file_response.path, r->pathname, strlen(r->pathname) + 1);
                                    memcpy(file_response.content, r->content, r->size);

                                    printf("sending file...\n");
                                    writen(incoming_request.fd_cleint, &file_response, sizeof(file_response));

                                    server_stat.actual_capacity -= r->size;
                                    server_stat.actual_max_files--;
                                    ht_delete(&storage_ht, r->pathname);

                                    n_to_eject--;
                                }
                            }
                        }
                    }

                    server_stat.actual_capacity += rec->size;
                    server_stat.actual_max_files++;

                    Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");

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
                        server_stat.actual_max_files--;
                        server_stat.actual_capacity -= rec->size;

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
                    server_stat.actual_max_files--;
                    server_stat.actual_capacity -= rec->size;

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
        Pthread_mutex_lock_(&mwpipe_mutex, "mw pipe");

        while (can_pipe == FALSE)
            Pthread_cond_wait(&workers_done, &mwpipe_mutex);

        can_pipe = FALSE;

        int res;
        printf("\n<<<riascolto al fd %d>>>\n", incoming_request.fd_cleint);
        if ((res = writen(mwpipe[1], &(incoming_request.fd_cleint), sizeof(int))) == -1)
        {
            perror("Writing on pipe error");
            exit(EXIT_FAILURE);
        }

        Pthread_mutex_unlock_(&mwpipe_mutex, "mw pipe");

        printf("wrote on pipe\n");
    }

    return NULL;
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

int select_lru_victims(size_t incoming_req_size, char *incoming_path)
{
    size_t sim_storage_size = server_stat.actual_capacity;
    int sim_storage_count = server_stat.actual_max_files;
    int n_to_eject = 0;

    /* making a cycle to determine how many files need to be ejected
                       to make space for the new file */
    while ((incoming_req_size + sim_storage_size) > server_stat.capacity || (1 + sim_storage_count) > (server_stat.max_files))
    {
        char oldest_path[MAX_PATHNAME];

        if (lru(storage_ht, oldest_path, incoming_path) == -1)
        {
            printf("\n<<<STORAGE IS EMPTY => FILE IS TOO BIG>>>");
            return -1;
        }

        printf("\n>>>victim selected: %s<<<\n", oldest_path);

        FRecord *victim = (FRecord *)ht_get(storage_ht, oldest_path);

        victim->is_victim = TRUE;

        printf("\n>>>sim actual storage size = %ld<<<\n", sim_storage_size);
        sim_storage_size -= victim->size;
        sim_storage_count--;
        n_to_eject++;
    }

    return n_to_eject;
}

int lru(HashTable ht, char *oldest_path, char *incoming_path)
{
    if (storage_ht.size == 0)
        return -1;

    /* most recent time */
    time_t oldest;
    time(&oldest);
    int found = -1;

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
                    (strncmp(rec->pathname, incoming_path, strlen(incoming_path)) != 0))
                {
                    /* found a later time than oldest */
                    oldest = rec->last_edit;
                    strncpy(oldest_path, rec->pathname, strlen(rec->pathname) + 1);
                    found = 1;
                }
            }
        }
    }

    return found;
}

void clean_all(pthread_t **workers_tid, int *fd_socket, int *fd_client)
{
    destroyQueue(pending_requests);
    // free_table(storage_ht);

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