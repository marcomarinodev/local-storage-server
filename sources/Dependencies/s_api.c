#include "s_api.h"
#include <time.h>

extern int fd_socket;

int openConnection(const char *sockname, int msec, const struct timespec abstime)
{
    struct sockaddr_un sa;
    time_t rawtime;

    strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    SYSCALL(fd_socket, socket(AF_UNIX, SOCK_STREAM, 0), "client socket");

    time(&rawtime);

    printf("first connection attempt at %s with fd = %d\n", sockname, fd_socket);
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
        else
        {
            printf("server is not online\n");
            return -1;
        }
    }

    printf("(%d) - Connected at '%s'\n", getpid(), sockname);

    return 0;
}

int closeConnection(const char *sockname)
{
    close(fd_socket);

    return 0;
}

int openFile(const char *pathname, int flags)
{
    Response response;
    ServerRequest request;
    long int file_size;
    int res;

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

    if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
    {
        errno = READN_ERR;
        return -1;
    }

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

        if (response.code == FILE_ALREADY_EXISTS)
        {
            printf("\n>>>FILE ALREADY EXISTS<<<\n");
            errno = FILE_ALREADY_EXISTS;
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
    int res, req;

    /* request init */
    memset(&request, 0, sizeof(ServerRequest));
    request.calling_client = getpid();
    request.cmd_type = REMOVE_FILE_REQ;
    memset(request.pathname, 0, MAX_PATHNAME);
    strncpy(request.pathname, pathname, strlen(pathname));
    request.fd_cleint = 0;

    printf("sending...\n");
    if ((req = writen(fd_socket, &request, sizeof(ServerRequest))) == -1)
    {
        errno = WRITEN_ERR;
        return -1;
    }

    printf("receiving...\n");
    if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
    {
        errno = READN_ERR;
        return -1;
    }

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
    int res, req;

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

    printf("sending...\n");

    if ((req = writen(fd_socket, &request, sizeof(ServerRequest))) == -1)
    {
        errno = WRITEN_ERR;
        return -1;
    }

    printf("receiving...\n");

    if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
    {
        errno = READN_ERR;
        return -1;
    }

    if (response.code == READ_SUCCESS)
    {
        (*size) = response.content_size;
        (*buf) = calloc(response.content_size + 1, sizeof(char));

        memcpy(*buf, response.content, *size);

        // printf("\n*** %s with sizeof %ld***\n", response.path, response.content_size);
        // write(STDOUT_FILENO, response.content, response.content_size * sizeof(char));
        // printf("\n********************\n");

        return 0;
    }

    return -1;
}

int closeFile(const char *pathname)
{
    Response response;
    ServerRequest request;
    int res, req;

    memset(&request, 0, sizeof(ServerRequest));
    request.calling_client = getpid();
    request.cmd_type = CLOSE_FILE_REQ;
    memset(request.pathname, 0, MAX_PATHNAME);
    strncpy(request.pathname, pathname, strlen(pathname));
    request.flags = NO_FLAGS;
    request.fd_cleint = 0;

    if ((req = writen(fd_socket, &request, sizeof(ServerRequest))) == -1)
    {
        errno = WRITEN_ERR;
        return -1;
    }

    if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
    {
        errno = READN_ERR;
        return -1;
    }

    printf("close file response from server\n");

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
    memset(request.content, 0, MAX_CHARACTERS);

    // memset(buffer, 0, 1 + file_size);

    /* file scan start */
    int fd = open(request.pathname, (O_RDWR | O_APPEND));
    read(fd, request.content, file_size);
    close(fd);
    /* file scan end */

    printf("FILE TO SEND = %s WITH SIZE %ld\n", request.content, request.size);

    int sending_err, res;

    if (fd_is_valid(fd_socket) == 1)
        printf("sending... to %d\n", fd_socket);
    else
        printf("fd_socket not valid\n");

    if ((sending_err = writen(fd_socket, &request, sizeof(ServerRequest))) == -1)
    {
        printf("(%d) - socket request write error\n", getpid());
        return -1;
    }

    printf("--- reading to socket\n");
    /* put the n_to_eject inside response.code */
    if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
    {
        errno = READN_ERR;
        return -1;
    }

    int n_ejected = response.code;
    printf("\n>>>Ejected files: %d<<<\n", n_ejected);

    if (n_ejected > 0)
    {

        /* expecting response.code files to be ejected */
        for (int i = 0; i < n_ejected; i++)
        {
            if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
            {
                errno = READN_ERR;
                return -1;
            }

            /* put ejected files into dirname if it's specified */
            if (dirname != NULL)
            {
                for (int i = 0; i < n_ejected; i++)
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

                    int fd;
                    
                    SYSCALL(fd, open(dir_abs_path, (O_RDWR | O_CREAT)), "open error");
                    SYSCALL(fd, write(fd, response.content, response.content_size * sizeof(char)), "write error");
                    SYSCALL(fd, close(fd), "close error");
                }
            }
        }
    }

    if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
    {
        errno = READN_ERR;
        return -1;
    }
    printf("\n>>>Reading the final response<<<\n");

    if (response.code == WRITE_SUCCESS)
        return 0;

    return -1;
}

int fd_is_valid(int fd)
{
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname)
{
    if (pathname == NULL)
    {
        errno = INVALID_PATHNAME;
        return -1;
    }

    /* sends the size with request.size = size */
    ServerRequest request;
    Response response;

    /* Clear node struct to suppress valgrind warnings */
    memset(&request, 0, sizeof(ServerRequest));
    request.calling_client = getpid();
    request.cmd_type = APPEND_FILE_REQ;
    memset(request.pathname, 0, MAX_PATHNAME);
    strncpy(request.pathname, pathname, strlen(pathname));
    request.size = size;
    memset(request.content, 0, MAX_CHARACTERS);
    memcpy(request.content, buf, size);

    int sending_err, res;

    printf("sending...\n");
    if ((sending_err = writen(fd_socket, &request, sizeof(ServerRequest))) == -1)
    {
        printf("(%d) - socket request write error\n", getpid());
        errno = WRITEN_ERR;
        return -1;
    }

    /* put the n_to_eject inside response.code */
    if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
    {
        errno = READN_ERR;
        return -1;
    }

    int n_ejected = response.code;
    printf("\n>>>Ejected files: %d<<<\n", response.code);

    if (n_ejected == 0)
    {
        Response ejectedFiles[n_ejected];

        /* expecting response.code files to be ejected */
        for (int i = 0; i < n_ejected; i++)
        {
            if ((res = readn(fd_socket, &ejectedFiles[i], sizeof(ejectedFiles[i]))) == -1)
            {
                errno = READN_ERR;
                return -1;
            }
        }

        if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
        {
            errno = READN_ERR;
            return -1;
        }

        printf("\n>>>Reading the final response<<<\n");

        /* put ejected files into dirname if it's specified */
        if (dirname != NULL)
        {
            for (int i = 0; i < n_ejected; i++)
            {
                char filename[MAX_PATHNAME];
                char abs_path_copy[MAX_PATHNAME];
                char *token;
                strncpy(abs_path_copy, ejectedFiles[i].path, strlen(ejectedFiles[i].path) + 1);

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

                int fd, res;

                SYSCALL(fd, open(dir_abs_path, (O_RDWR | O_CREAT)), "open error");

                SYSCALL(res, write(fd, ejectedFiles[i].content, ejectedFiles[i].content_size * sizeof(char)), "write error");

                SYSCALL(res, close(fd), "close error");
            }
        }
    }

    if (response.code == APPEND_FILE_SUCCESS)
        return 0;

    return -1;
}

int readNFiles(int n, const char *dirname)
{
    if (dirname == NULL)
    {
        errno = INVALID_PATHNAME;
        return -1;
    }

    Response response;
    ServerRequest request;
    int res;

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
    if ((res = writen(fd_socket, &request, sizeof(request))) == -1)
    {
        errno = WRITEN_ERR;
        return -1;
    }

    /* right here the client knows how many files can request to read */
    if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
    {
        errno = READN_ERR;
        return -1;
    }

    printf("\n<<<info sent, now i'm waiting %d files>>>\n", response.code);

    int n_to_read = response.code;

    for (int i = 0; i < n_to_read; i++) /* using the response code as number of files to read */
    {
        /* listen to the server to a file in storage */
        printf("\n<<<waiting for %d file>>>\n", i);
        if ((res = readn(fd_socket, &response, sizeof(response))) == -1)
        {
            errno = READN_ERR;
            return -1;
        }

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

                // int fd = open(dir_abs_path, (O_RDWR | O_CREAT));
                int fd;
                SYSCALL(fd, open(dir_abs_path, (O_RDWR | O_CREAT)), "open error");

                int res;
                SYSCALL(res, write(fd, response.content, response.content_size * sizeof(char)), "write error");

                SYSCALL(res, close(fd), "close error");
            }
            else /* dirname is not specified, so we basically print out the content */
            {
                int res;
                printf("\n*** %s ***\n", response.path);
                if ((res = write(STDOUT_FILENO, response.content, response.content_size * sizeof(char))) == -1)
                {
                    errno = UNKNOWN_ERROR;
                    return -1;
                }

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