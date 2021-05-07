#define SYSCALL(r, c, e) \
    if ((r = c) == -1)   \
    {                    \
        perror(e);       \
        exit(errno);     \
    }

#define SYSCALL_FILE(r, c, e) \
    if ((r = c) == NULL)   \
    {                    \
        perror(e);       \
        exit(errno);     \
    }
