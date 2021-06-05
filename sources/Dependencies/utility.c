#include "utility.h"

int str_toint(char* string)
{   
    char* endptr = NULL;
    int result = 0;

    result = strtol(string, &endptr, 10);
    
    return result;
}