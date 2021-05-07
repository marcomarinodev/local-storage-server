#include "arg_parser.h"

void get_multiargs(struct queue* args_queue, char *arg_string)
{
    char *token = strtok(arg_string, ",");

    while (token != NULL)
    {
        enqueue(args_queue, token);
        token = strtok(NULL, ",");
    }
}

// --- USAGE ---
// int main() 
// {
//     char str[] = "file1,file2,file3";
//     struct queue* args = create_queue();

//     get_multiargs(args, str);

//     while (args->front != NULL)
//     {
//         printf("ARG: - %s\n", (char *)args->front->key);
//         dequeue(args);
//     }

//     return 0;
// }