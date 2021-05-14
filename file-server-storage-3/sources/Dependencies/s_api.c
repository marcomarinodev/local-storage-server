#include "s_api.h"
#include <time.h>

extern int fd_socket;

int openConnection(const char *sockname, int msec, const struct timespec abstime)
{
    // FD_SOCKET DOVREBBE ESSERE QUI? OPPURE DOVREBBE ESSERE CHIAMATO GLOBALMENTE?
    // int fd_socket
    struct sockaddr_un sa;
    time_t rawtime;

    strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    SYSCALL(fd_socket, socket(AF_UNIX, SOCK_STREAM, 0), "client socket");

    time(&rawtime);

    while (connect(fd_socket, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        if (errno == ENOENT)
        {
            if (rawtime > abstime.tv_sec)
            {
                printf("(%d) - Ttime out: connection failed\n", getpid());
                errno = ETIMEDOUT;
                return -1;
            }

            printf("(%d) - Waiting for connection...\n", getpid());
            time(&rawtime);

            usleep(msec * 1000);
        }
    }

    printf("(%d) - Connected at '%s'\n", getpid(), sockname);

    return 0;
}

int closeConnection(const char *sockname)
{
    printf("(%d) - Closing connection at '%s'\n", getpid(), sockname);
    return close(fd_socket);
}

int openFile(const char *pathname, int flags)
{
    Request_tosend *request = (Request_tosend *)malloc(sizeof(Request_tosend));
    long int file_size;

    if ((file_size = find_size(pathname)) == -1)
    {
        free(request);
        return -1;
    }

    switch (flags)
    {
    case O_LOCK:
        break;
    case O_CREATE:
        break;
    case O_LOCK | O_CREATE:
        break;
    case NO_FLAGS:
        break;
    default:
        printf("(%d) - [openFile] - Flag %d not recognized\n", getpid(), flags);
        free(request);
        return -1;
        break;
    }

    request->calling_client = getpid();
    request->cmd_type = OPEN_FILE_REQ;
    request->pathname = (char *)calloc(strlen(pathname) + 1, sizeof(char));

    sprintf(request->pathname, "%s", pathname);

    request->flags = flags;
    request->size = file_size;
    request->content = NULL;

    send_req(request);

    char buffer[3];
    read(fd_socket, buffer, 3);

    if (strcmp(OPEN_SUCCESS, buffer) == 0)
    {
        printf("\nOPEN_SUCCESS\n");
        return 0;
    }

    if (strcmp(O_CREATE_SUCCESS, buffer) == 0)
    {
        printf("\nOPEN_SUCCESS\n");
        return 0;
    }

    return -1;
}

int send_req(Request_tosend *r)
{
    char *req;
    int sending_err;

    if (r->content != NULL)
    {
        req = (char *)calloc(MAX_CHARACTERS + MAX_REQUEST, sizeof(char));
        sprintf(req, "%d|%d|%s|%d|%ld|%s", r->calling_client,
                r->cmd_type, r->pathname, r->flags, r->size, r->content);
    }
    else
    {
        req = (char *)calloc(MAX_REQUEST, sizeof(char));
        sprintf(req, "%d|%d|%s|%d|%ld|nil", r->calling_client,
                r->cmd_type, r->pathname, r->flags, r->size);
    }

    int n = strlen(req);

    printf("\nlunghezza della richiesta = %d\n", n);

    if ((sending_err = writen(fd_socket, &n, sizeof(int)) == -1))
    {
        printf("(%d) - socket request write error\n", getpid());
        return -1;
    }

    printf("(CLIENT) Attempting to send:\n%s\n", req);

    if ((sending_err = writen(fd_socket, req, strlen(req) * sizeof(char))) == -1)
    {
        printf("(%d) - socket request write error\n", getpid());
        return -1;
    }

    free(req);
    free(r->pathname);

    if (r->content != NULL)
        free(r->content);

    free(r);

    return 0;
}

int closeFile(const char *pathname)
{
    return 0;
}

int writeFile(const char *pathname, const char *dirname)
{
    Request_tosend *request = (Request_tosend *)malloc(sizeof(Request_tosend));

    if (request == NULL)
    {
        perror("memory fatal error");
        exit(EXIT_FAILURE);
    }

    FILE *fp;
    char line[800];
    long int file_size;

    if ((file_size = find_size(pathname)) == -1)
    {
        free(request);
        return -1;
    }

    request->calling_client = getpid();
    request->cmd_type = WRITE_FILE_REQ;
    request->pathname = (char *)malloc((strlen(pathname) * sizeof(char)) + 1);
    strncpy(request->pathname, pathname, strlen(pathname) + 1);
    request->flags = NO_FLAGS;
    request->size = file_size;
    request->content = (char *)malloc(MAX_CHARACTERS * sizeof(char));

    if (request->content == NULL)
    {
        perror("memory fatal error");
        exit(EXIT_FAILURE);
    }

    /* put file content inside request->content */
    fp = fopen(request->pathname, "r");

    if (fp == NULL)
    {
        printf("Could not open file %s", request->pathname);
        return -1;
    }

    int i = 0;
    while (fgets(line, 800, fp) != NULL)
    {
        printf("LINE LEN: %ld\n", strlen(line));
        if (i == 0)
        {
            strncpy(request->content, line, strlen(line) + 1);
        }
        else
            strncat(request->content, line, strlen(line) + 1);

        i++;
    }

    fclose(fp);

    send_req(request);

    char buffer[3];
    read(fd_socket, buffer, 3);

    printf("\n%s\n", buffer);

    if (strcmp(WRITE_SUCCESS, buffer) == 0)
        return 0;

    return 0;
}

int readFile(const char *pathname, void **buf, size_t *size)
{
    printf("reading abs path: %s\n", pathname);
    return 0;
}

long int find_size(const char *pathname)
{
    // opening the file in read mode
    FILE *fp = fopen(pathname, "r");

    // checking if the file exist or not
    if (fp == NULL)
    {
        printf("(%d) - File Not Found!\n", getpid());
        return -1;
    }

    fseek(fp, 0L, SEEK_END);

    long int res = ftell(fp);

    fclose(fp);

    return res;
}

char **parse_pipe_args(char *arguments, int n_args, int lengths[])
{
    char **strings = malloc(sizeof(char *) * n_args);

    char *token = strtok(arguments, "|");

    for (int i = 0; i < n_args; i++)
    {
        if (token != NULL)
        {
            strings[i] = (char *)malloc(sizeof(char) * lengths[i]);
            strncpy(strings[i], token, strlen(token) + 1);
            if (i != n_args - 1)
                token = strtok(NULL, "|");
        }
    }

    return strings;
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