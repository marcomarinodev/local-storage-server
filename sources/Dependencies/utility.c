#include "utility.h"

int str_toint(char *string)
{
    char *endptr = NULL;
    int result = 0;

    result = strtol(string, &endptr, 10);

    return result;
}

void *safe_malloc(size_t mem_size)
{
    void *ptr = malloc(mem_size);

    if (ptr == NULL)
    {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    memset(ptr, 0, mem_size);

    return ptr;
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