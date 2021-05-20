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
    Response response;
    ServerRequest request;
    long int file_size;

    if ((file_size = find_size(pathname)) == -1)
    {
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
        return -1;
        break;
    }

    /* Clear node struct to suppress valgrind warnings */
    memset(&request, 0, sizeof(ServerRequest));

    request.calling_client = getpid();
    request.cmd_type = OPEN_FILE_REQ;
    memset(request.pathname, 0, MAX_PATHNAME);
    strncpy(request.pathname, pathname, strlen(pathname));

    request.flags = flags;
    request.size = file_size;
    request.fd_cleint = 0;

    memset(request.content, 0, MAX_CHARACTERS);
    sprintf(request.content, "%s", " ");

    int sending_err = 0;

    printf("sending...\n");
    if ((sending_err = writen(fd_socket, &request, sizeof(ServerRequest))) == -1)
    {
        printf("(%d) - socket request write error\n", getpid());
        return -1;
    }

    int read_op = readn(fd_socket, &response, sizeof(response));

    if (errno != EINTR)
    {
        if (response.code == OPEN_SUCCESS)
        {
            printf("\n<<<OPEN_SUCCESS>>>\n");
            return 0;
        }

        if (response.code == O_CREATE_SUCCESS)
        {
            printf("\n<<<OPEN (O_CREATE) SUCCESS>>>\n");
            return 0;
        }

        if (response.code == IS_ALREADY_OPEN)
        {
            printf("\n<<<THE FILE IS ALREADY OPEN!>>>\n");
            return 0;
        }

        if (response.code == FILE_IS_LOCKED)
        {
            printf("\n<<<YOU CANNOT OPEN THE FILE BECAUSE IT IS LOCKED>>>\n");
            errno = FILE_IS_LOCKED;
            return -1;
        }

        if (response.code == STRG_OVERFLOW)
        {
            printf("\n<<<STORAGE OVERFLOW (file is too big)>>>\n");
            errno = STRG_OVERFLOW;
            return -1;
        }
    }

    /* errno = EINTR */
    return -1;
}

int removeFile(const char *pathname)
{
    ServerRequest request;
    Response response;

    /* request init */
    memset(&request, 0, sizeof(ServerRequest));
    request.calling_client = getpid();
    request.cmd_type = REMOVE_FILE_REQ;
    memset(request.pathname, 0, MAX_PATHNAME);
    strncpy(request.pathname, pathname, strlen(pathname));
    request.fd_cleint = 0;

    printf("sending...\n");
    writen(fd_socket, &request, sizeof(ServerRequest));

    printf("receiving...\n");
    readn(fd_socket, &response, sizeof(response));

    /* response.code handling */
    if (errno != EINTR)
    {
        if (response.code == REMOVE_FILE_SUCCESS)
            return 0;
        
        if (response.code == FAILED_FILE_SEARCH)
        {
            errno = FAILED_FILE_SEARCH;
            return -1;
        }

        if (response.code == FILE_IS_LOCKED)
        {
            errno = FILE_IS_LOCKED;
            return -1;
        }
    }

    /* errno = EINTR or some other error */
    return -1;
}

int readFile(const char *pathname, void **buf, size_t *size)
{
    ServerRequest request;
    Response response;

    memset(&request, 0, sizeof(ServerRequest));
    request.calling_client = getpid();
    request.cmd_type = READ_FILE_REQ;
    memset(request.pathname, 0, MAX_PATHNAME);
    strncpy(request.pathname, pathname, strlen(pathname));
    request.flags = NO_FLAGS;
    request.size = 0;
    request.fd_cleint = 0;
    memset(request.content, 0, MAX_CHARACTERS);
    sprintf(request.content, "%s", "/");

    printf("reading abs path: %s\n", pathname);
    int sending_err = 0;

    printf("sending...\n");

    writen(fd_socket, &request, sizeof(ServerRequest));

    printf("receiving...\n");

    readn(fd_socket, &response, sizeof(response));

    if (response.code == READ_SUCCESS)
    {
        (*size) = response.content_size;
        (*buf) = calloc(response.content_size + 1, sizeof(char));

        memcpy(*buf, response.content, *size);

        printf("\n*** %s ***\n", response.path);
        write(STDOUT_FILENO, response.content, response.content_size * sizeof(char));
        printf("\n********************\n");

        return 0;
    }

    return -1;
}

int closeFile(const char *pathname)
{
    Response response;
    ServerRequest request;

    memset(&request, 0, sizeof(ServerRequest));
    request.calling_client = getpid();
    request.cmd_type = CLOSE_FILE_REQ;
    memset(request.pathname, 0, MAX_PATHNAME);
    strncpy(request.pathname, pathname, strlen(pathname));
    request.flags = NO_FLAGS;
    request.fd_cleint = 0;

    writen(fd_socket, &request, sizeof(ServerRequest));

    readn(fd_socket, &response, sizeof(response));

    if (errno != EINTR)
    {
        if (response.code == CLOSE_FILE_SUCCESS)
            return 0;
        
        if (response.code == IS_ALREADY_CLOSED)
            return 0;

        if (response.code == FILE_IS_LOCKED)
        {
            errno = response.code;
            return -1;
        }

        if (response.code == FAILED_FILE_SEARCH)
        {
            errno = response.code;
            return -1;
        }
    }


    /* errno = EINTR */
    return -1;
}

int writeFile(const char *pathname, const char *dirname)
{
    Response response;
    ServerRequest request;
    long int file_size;

    if ((file_size = find_size(pathname)) == -1)
    {
        return -1;
    }

    /* Clear node struct to suppress valgrind warnings */
    memset(&request, 0, sizeof(ServerRequest));
    request.calling_client = getpid();
    request.cmd_type = WRITE_FILE_REQ;
    memset(request.pathname, 0, MAX_PATHNAME);
    strncpy(request.pathname, pathname, strlen(pathname));
    request.flags = NO_FLAGS;
    request.size = file_size;
    request.fd_cleint = 0;

    int fd = open(request.pathname, (O_RDWR | O_APPEND));
    memset(request.content, 0, MAX_CHARACTERS);

    read(fd, request.content, file_size);

    close(fd);

    printf("FILE TO SEND = %s WITH SIZE %ld\n", request.content, request.size);

    int sending_err = 0;

    printf("sending...\n");
    if ((sending_err = writen(fd_socket, &request, sizeof(ServerRequest))) == -1)
    {
        printf("(%d) - socket request write error\n", getpid());
        return -1;
    }

    readn(fd_socket, &response, sizeof(response));

    if (response.code == WRITE_SUCCESS)
        return 0;

    return -1;
}

int readNFiles(int n, const char *dirname)
{
    Response response;
    ServerRequest request;

    /* Clear node struct to suppress valgrind warnings */
    memset(&request, 0, sizeof(ServerRequest));
    request.calling_client = getpid();
    request.cmd_type = READN_FILE_REQ;
    memset(request.pathname, 0, MAX_PATHNAME);
    sprintf(request.pathname, "%s", "/");
    memset(request.content, 0, MAX_CHARACTERS);
    sprintf(request.content, "%s", "/");
    request.flags = n; /* using flags to indicate the number of files to read */
    request.size = 0;
    request.fd_cleint = 0;

    /* sending how many file the client wants to read */
    writen(fd_socket, &request, sizeof(request));

    /* right here the client knows how many files can request to read */
    readn(fd_socket, &response, sizeof(response));

    printf("\n<<<info sent, now i'm waiting %d files>>>\n", response.code);

    int n_to_read = response.code;

    for (int i = 0; i < n_to_read; i++) /* using the response code as number of files to read */
    {
        /* listen to the server to a file in storage */
        printf("\n<<<waiting for %d file>>>\n", i);
        readn(fd_socket, &response, sizeof(response));

        if (response.code == READ_SUCCESS)
        {
            printf("\n<<Read success by server>>\n");
            if (dirname != NULL) /* case when dirname is specified so we need to write into dirname */
            {
                char filename[MAX_PATHNAME];
                char abs_path_copy[MAX_PATHNAME];
                char *token;
                strncpy(abs_path_copy, response.path, strlen(response.path) + 1);

                token = strtok(abs_path_copy, "/");

                while (token != NULL)
                {
                    sprintf(filename, "%s", token);
                    token = strtok(NULL, "/");
                }

                char dir_abs_path[MAX_PATHNAME];

                realpath(dirname, dir_abs_path);

                strcat(dir_abs_path, "/");
                strcat(dir_abs_path, filename);

                int fd = open(dir_abs_path, (O_RDWR | O_CREAT));

                write(fd, response.content, response.content_size * sizeof(char));

                close(fd);
            }
            else /* dirname is not specified, so we basically print out the content */
            {
                printf("\n*** %s ***\n", response.path);
                write(STDOUT_FILENO, response.content, response.content_size * sizeof(char));
                printf("\n********************\n");
            }
        }
        else
            return -1;
    }

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