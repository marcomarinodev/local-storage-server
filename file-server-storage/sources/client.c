#include "client.h"

int fd_socket;

// static char server_socket_pathname[80];

int main(int argc, char *argv[])
{
    
    // check_argc(argc);

    // int opt;
    // int _exit = EXIT_SUCCESS;

    // while ((opt = getopt(argc, argv, ":hf:")) != -1)
    // {
    //     switch (opt)
    //     {
    //     case 'h':
    //     {
    //         // help 
    //         printf("\n => usage: %s\n   -h <help> -f <filename> -w <dirname[,n=0]>\n   -W <file1[,file2]> -r <file1,[,file2] -R <int>\n   -d <dirname> -t <time> -l <file1[,file2]>\n   -u <file1[,file2]> -c <file1[,file2]> -p\n", argv[0]);
    //         return EXIT_SUCCESS;
    //     }
    //     break;
    //     case 'f':
    //     {
    //         //  setta il path del server_socket 
    //         strncpy(server_socket_pathname, optarg, strlen(optarg));
    //         printf("comando -f: il server socket si trova qui -> %s\n", server_socket_pathname);
    //     }
    //     break;
    //     case 'w':
    //     {
            
            
    //     }
    //     break;
    //     case ':':
    //     {
    //         printf("l'opzione '-%c' richiede un argomento\n", optopt);
    //         _exit = EXIT_FAILURE;
    //     }
    //     break;
    //     case '?':
    //     {
    //         printf("l'opzione '-%c' non esiste\n", optopt);
    //         _exit = EXIT_FAILURE;
    //     }
    //     default:
    //     {
    //         _exit = EXIT_FAILURE;
    //     }
    //     break;
    //     }
    // }




    
    struct timespec abs_time;
    int msec_waiting = 1500;

    abs_time.tv_sec = (time_t)(msec_waiting / 1000);
    abs_time.tv_nsec = (msec_waiting % 1000) * 1000000;

    if (openConnection(SERVER_SOCKET_PATH, 100, abs_time) == -1)
    {
        perror("Waiting Server time out (Client)\n");
        exit(EXIT_FAILURE);
    }

    // TEMPORARY. THIS CODE NEED TO BE REPLACED 
    // ----------------------- 
    write(fd_socket, "Hallo!", 7);
    printf("Write fatta (Client)\n");
    char buffer[30];

    read(fd_socket, buffer, 30);

    printf("Client got: %s\n", buffer);
    // ----------------------- 

    if (closeConnection(SERVER_SOCKET_PATH) == -1)
    {
        perror("Closing Connection failed (Client)\n");
        exit(EXIT_FAILURE);
    } 

    // return _exit;
    return 0;
}

void check_argc(int arg_c)
{
    if (arg_c == 1)
    {
        printf("Inserire almeno un argomento per iniziare!\n");
        exit(EXIT_FAILURE);
    }
}