#include "utility.h"

int str_toint(char* string)
{   
    char* endptr = NULL;
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