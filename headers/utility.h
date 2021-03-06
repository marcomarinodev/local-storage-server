#if !defined(UTILITY_H_)
#define UTILITY_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>

#define SYSCALL_EXIT(name, r, sc, str, ...) \
    if ((r = sc) == -1)                     \
    {                                       \
        perror(#name);                      \
        int errno_copy = errno;             \
        print_error(str, __VA_ARGS__);      \
        exit(errno_copy);                   \
    }

#define COMM(res)                      \
    if (res < 0)                       \
    {                                  \
        printf("communication error"); \
        break;                         \
    }

static inline void print_error(const char *str, ...)
{
    const char err[] = "ERROR: ";
    va_list argp;
    char *p = (char *)malloc(strlen(str) + strlen(err) + 512);
    if (!p)
    {
        perror("malloc");
        fprintf(stderr, "FATAL ERROR nella funzione 'print_error'\n");
        return;
    }
    strcpy(p, err);
    strcpy(p + strlen(err), str);
    va_start(argp, str);
    vfprintf(stderr, p, argp);
    va_end(argp);
    free(p);
}

int str_toint(char *string);

void *safe_malloc(size_t mem_size);

ssize_t readn(int fd, void *ptr, size_t n);

ssize_t writen(int fd, void *ptr, size_t n);

#endif