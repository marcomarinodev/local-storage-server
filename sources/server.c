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

/* CONNECTED CLIENTS */
LList fds_client;
pthread_mutex_t fds_client_mutex = PTHREAD_MUTEX_INITIALIZER;

/* SERVER SOCKET FILE DESCRIPTOR */
int fd_socket;

/* DESCRIPTORS SET */
fd_set set;
pthread_mutex_t descriptors_set = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
    check_argc(argc);

    run_server(argv[1]);

    return 0;
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
    /* mantengo il massimo indice di descrittore attivo in fd_num */
    if (fd_socket > fd_num)
        fd_num = fd_socket;
    FD_ZERO(&set);
    FD_SET(fd_socket, &set);

    // INIT
    pending_requests = createQueue(sizeof(ServerRequest));

    save_setup(&server_setup);
    storage_ht = create_table(1 + (server_setup->max_files_instorage / 2), server_setup->max_storage);

    workers_tid = (pthread_t *)malloc(sizeof(pthread_t) * server_setup->n_workers);
    for (int i = 0; i < server_setup->n_workers; i++)
    {
        Pthread_create(&workers_tid[i], NULL, &worker_func, NULL);
    }

    while (1)
    {
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
}

void check_argc(int argc)
{
    if (argc == 1)
    {
        printf("You have to insert the configuration file\n");
        exit(EXIT_FAILURE);
    }
}

void *worker_func(void *args)
{
    /* in this function the server will parse the client request */

    /* worker will read the request from fd_client */

    ServerRequest incoming_request;
    memset(&incoming_request, 0, sizeof(ServerRequest));

    printf("--- thread %ld attivo ---\n", pthread_self());

    while (1)
    {
        Pthread_mutex_lock(&pending_requests_mutex, pthread_self(), "requests queue");

        while (isEmpty(pending_requests) == 1)
            Pthread_cond_wait(&pending_requests_cond, &pending_requests_mutex);

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

                printf("{{{ incoming_request.size = %ld }}}\n", incoming_request.size);
                new.content = malloc(1 + (sizeof(char) * incoming_request.size));
                memcpy(new.content, incoming_request.content, incoming_request.size);
                

                Pthread_mutex_lock(&storage_mutex, pthread_self(), "storage");

                FRecord *rec = ht_search(storage_ht, incoming_request.pathname);

                if (rec != NULL) /* file already exists in storage */
                {
                    Pthread_mutex_unlock(&storage_mutex, pthread_self(), "storage");

                    Pthread_mutex_lock(&(rec->lock), pthread_self(), rec->pathname);

                    rec->is_locked = 1;
                    rec->is_open = 1;
                    response.code = OPEN_SUCCESS;
                    free(new.content);

                    Pthread_mutex_unlock(&(rec->lock), pthread_self(), rec->pathname);
                }
                else /* file does not exists */
                {
                    if (incoming_request.size <= storage_ht->capacity)
                    { /* case when storage can contain the file sent in request */
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

                        pthread_mutex_init(&(new.lock), NULL);
                        new.is_open = TRUE;
                        new.is_locked = TRUE;

                        ht_insert(storage_ht, incoming_request.pathname, new, incoming_request.size);

                        free(new.content);
                        
                        response.code = O_CREATE_SUCCESS;
                    }
                    else
                    {
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
                if (rec->is_open == TRUE)
                {
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

        default:
            break;
        }

        print_table(storage_ht);

        Pthread_mutex_lock(&descriptors_set, pthread_self(), "descriptors set");
        FD_SET(incoming_request.fd_cleint, &set);
        Pthread_mutex_unlock(&descriptors_set, pthread_self(), "descriptors set");
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