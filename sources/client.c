#include "client.h"

int fd_socket;
Client_setup c_setup;

int main(int argc, char *argv[])
{
    check_argc(argc);

    LList config_commands;
    LList request_commands;

    LL_init(&config_commands, print_config_node);
    LL_init(&request_commands, print_request_node);

    if ((_getopt(&config_commands, &request_commands, argc, argv)) == 0)
    {
        c_setup = apply_setup(config_commands);

        struct timespec abs_time;
        time(&abs_time.tv_sec);
        abs_time.tv_sec += 20;

        if (openConnection(c_setup.socket_pathname, 1000, abs_time) == -1)
        {
            printf("(%d) - Waiting Server time out\n", getpid());
            exit(EXIT_FAILURE);
        }

        if (errno == 0)
        {
            int r;
            if ((r = perform(c_setup, &request_commands)) == NOSUCHFILE_ORDIR)
            {
                printf("(%d) - No such file or directory!\n", getpid());
                if (config_commands.head != NULL)
                    LL_free(config_commands, NULL);
                if (request_commands.head != NULL)
                    LL_free(request_commands, NULL);

                if (closeConnection(c_setup.socket_pathname) == -1)
                {
                    perror("Closing Connection failed (Client)\n");
                    exit(EXIT_FAILURE);
                }
                return errno;
            }
        }
        else
        {
            fprintf(stderr, "Setup error.\n");
            clean_options(&config_commands, &request_commands);
            return errno;
        }
    }

    if (fd_is_valid(fd_socket))
    {
        if (closeConnection(c_setup.socket_pathname) == -1)
        {
            perror("Closing Connection failed (Client)\n");
            clean_options(&config_commands, &request_commands);
            exit(EXIT_FAILURE);
        }
    }

    clean_options(&config_commands, &request_commands);

    return 0;
}

/**
 * ----------------
 * ----------------
 * ----------------
 * ------MAIN------
 * ----------------
 * ----------------
 * */

int fd_is_valid(int fd)
{
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

void clean_options(LList *configs, LList *requests)
{
    LL_free((*configs), NULL);
    LL_free((*requests), NULL);
}

int _getopt(LList *configs, LList *reqs, int argcount, char **_argv)
{
    int opt;
    Request *current_request = malloc(sizeof(Request));

    /* defaults p command set to 0 */
    char *opt_code = malloc(2 * sizeof(char));
    char *opt_value = malloc(30 * sizeof(char));

    sprintf(opt_code, "p");
    sprintf(opt_value, "%d", 0);
    LL_enqueue(configs, (void *)opt_value, opt_code);

    opt_code = malloc(2 * sizeof(char));
    opt_value = malloc(30 * sizeof(char));

    while ((opt = getopt(argcount, _argv, ":phf:w:W:D:R:r:d:t:l:u:c:")) != -1)
    {
        switch (opt)
        {
            /* config command types */
        case 'p':
            sprintf(opt_code, "p");
            sprintf(opt_value, "1");

            if (LL_contains_key(*configs, "p") == TRUE)
            {
                LL_remove_by_key(configs, "p");
                LL_enqueue(configs, (void *)opt_value, opt_code);
            }

            opt_code = malloc(sizeof(char) * 2);
            opt_value = malloc(sizeof(char) * 50);
            // -----------------------
            break;
        case 'h':
            print_help(_argv);

            free(opt_code);
            free(opt_value);
            free(current_request);
            return HELP_EXIT;
        case 'f':
            manage_config_option(&opt_code, &opt_value, opt, configs, optarg);
            break;
        case 'D':
            manage_config_option(&opt_code, &opt_value, opt, configs, optarg);
            break;
        case 't':
            manage_config_option(&opt_code, &opt_value, opt, configs, optarg);
            break;
        case 'd':
            manage_config_option(&opt_code, &opt_value, opt, configs, optarg);
            break;
            /* request command types */
        case 'R': /* it requires an optional argument */
            manage_request_option(&opt_code, opt, &current_request, reqs, optarg);
            break;
        case 'r':
            manage_request_option(&opt_code, opt, &current_request, reqs, optarg);
            break;
        case 'w':
            manage_request_option(&opt_code, opt, &current_request, reqs, optarg);
            break;
        case 'W':
            manage_request_option(&opt_code, opt, &current_request, reqs, optarg);
            break;
        case 'l':
            manage_request_option(&opt_code, opt, &current_request, reqs, optarg);
            break;
        case 'u':
            manage_request_option(&opt_code, opt, &current_request, reqs, optarg);
            break;
        case 'c':
            manage_request_option(&opt_code, opt, &current_request, reqs, optarg);
            break;
        case ':':

            if (optopt == 'R')
            {
                manage_request_option(&opt_code, 'R', &current_request, reqs, "-1");
                break;
            }
            else
            {
                printf("l'opzione '-%c' richiede un argomento\n", optopt);
                return MISSING_ARG;
            }

        case '?':
            printf("l'opzione '-%c' non esiste\n", optopt);
            return INVALID_OPT;
        default:
            break;
        }
    }

    free(opt_code);
    free(opt_value);
    free(current_request);

    return validate(*configs, *reqs);
}

int perform(Client_setup setup, LList *request_commands)
{
    print_setup(setup);
    print_requests(request_commands);

    Node *current_request_node = request_commands->head;

    while (current_request_node != NULL)
    {
        /* catching the request */
        Request *current_request = (Request *)(current_request_node->data);

        char operation = current_request->code;
        char **request_args = parse_comma_args(current_request->arguments);

        switch (operation)
        {
            /**
             * sends files inside dirname into the server, if n is specified, it 
             * represents the maximum number of file to send to the server. if n 
             * is not specified there's no upperbound.
            */
        case 'w':
        {
            /* we need to handle the second argument optionality */
            char *dirname = request_args[0];
            int n = EXAUSTIVE_READ;

            if (request_args[1] != NULL)
            {
                char *endptr = NULL;
                n = strtol(request_args[1], &endptr, 10);

                if ((n == 0 && errno != 0) || (n == 0 && endptr == request_args[1]))
                    return NAN_INPUT_ERROR;
                else
                {
                    if (n < 0)
                    {
                        fprintf(stderr, "n arg must be >= 0.\n");
                        return INVALID_NUMBER_INPUT_ERROR;
                    }
                }
            }

            /* if n it's specified with 0, we have to set it to EXAUSTIVE_READ */
            if (n == 0)
                n = EXAUSTIVE_READ;

            printf("-w command execution:\n");
            /* it navigates recursively through directories until n files written is reached */
            int r;
            if ((r = lsR(dirname, &n)) == NOSUCHFILE_ORDIR)
            {
                free(request_args);
                return NOSUCHFILE_ORDIR;
            }

            printf("(Directory scanning ok)\n");
        }
        break;

        case 'W':
        {
            int i = 0;

            while (request_args[i] != NULL)
            {
                char abs_path[MAXFILENAME];
                realpath(request_args[i], abs_path);

                PATH_CHECK(abs_path);

                printf("-W command execution:\n");
                /* if it passed the check, then we call the API in order to write into the storage */

                int open_res = openFile(abs_path, O_CREATE | O_LOCK);

                if (open_res == 0)
                {
                    sleep(3);
                    if (writeFile(abs_path, c_setup.ejected_buffer) == 0)
                    {
                        printf("\nSuccessful write operation \n");
                        printf("\nclosing the file...\n");
                        if (closeFile(abs_path) == 0)
                        {
                            printf("\nSuccesful closing operation\n");
                        }
                        else
                            printf("\n<<<CLOSE FAILED DUE TO (%s)>>>\n", translate_error_code(errno));
                    }
                    else
                    {
                        printf("WRITE FAILED\n");
                    }
                }
                else
                {
                    printf("\n<<<OPEN FAILED DUE TO (%s)>>>\n", translate_error_code(errno));

                    /* in order to test appendToFile */
                    if (errno == FILE_ALREADY_EXISTS)
                        appendToFile(abs_path, "   >>>hey hey hey sono un test<<<", 34, c_setup.ejected_buffer);

                }

                i++;
            }
        }
        break;

        case 'r':
        {
            int i = 0;

            printf("ARG[0] = %s\n", request_args[0]);

            while (request_args[i] != NULL)
            {
                size_t data_buffer_dim = MAX_FILE_SIZE;
                void *file_buffer;

                char abs_path[MAXFILENAME];

                realpath(request_args[i], abs_path);

                PATH_CHECK(abs_path);

                /* in order to save in -d dirname the read file, we have to take
                   the last token separated by / inside the abs path:
                   example: user wants to read a file in test_folder/test_01.txt
                            and put it into dirname, but in dirname we do not want
                            to write a subfolder (dirname/test_folder/test_01.txt).
                   so if I take the token inside abs_path the result will be 
                   (dirname/test_01.txt) while its path inside storage is
                   (.../test_folder/test_01.txt)
                */
                char abs_path_copy[MAXFILENAME];
                strncpy(abs_path_copy, abs_path, strlen(abs_path) + 1);
                char *token;
                char filename[200];

                token = strtok(abs_path_copy, "/");

                while (token != NULL)
                {
                    sprintf(filename, "%s", token);
                    token = strtok(NULL, "/");
                }

                printf("-r command execution:\n");

                int read_op = readFile(abs_path, &file_buffer, &data_buffer_dim);

                if (read_op == 0)
                {
                    /* parse file buffer and put it in a directory if
                   -d config is set, otherwise print out in stdout */
                    printf(" ==> %s\n", (char *)(file_buffer));

                    if (c_setup.dirname_buffer != NULL)
                    {
                        // write file to dirname
                        // we call the new file with request_args[i]
                        char dir_abs_path[MAXFILENAME];

                        realpath(c_setup.dirname_buffer, dir_abs_path);

                        strcat(dir_abs_path, "/");
                        strcat(dir_abs_path, filename);

                        printf("\n<<<DIRNAME TO INSERT THE READ FILE = %s>>>\n", dir_abs_path);

                        int fd = open(dir_abs_path, (O_RDWR | O_CREAT));

                        write(fd, file_buffer, data_buffer_dim * sizeof(char));

                        close(fd);
                    }

                    free(file_buffer);
                }
                else
                {
                    if (errno == FAILED_FILE_SEARCH)
                    {
                        printf("The file does not exists in storage\n");
                    }
                }

                i++;
            }
        }
        break;

        case 'R':
        {

            int n = EXAUSTIVE_READ;
            if (request_args[0] != NULL)
            {
                char *endptr = NULL;
                n = strtol(request_args[0], &endptr, 10);

                if ((n == 0 && errno != 0) || (n == 0 && endptr == request_args[1]))
                    return NAN_INPUT_ERROR;

                /* if n it's specified with 0, we have to set it to EXAUSTIVE_READ */
                if (n == 0)
                    n = EXAUSTIVE_READ;
            }

            int res;
            if ((res = readNFiles(n, c_setup.dirname_buffer)) == -1)
            {
                printf("some error occurred\n");
            }
        }
        break;

        case 'l':
        {
        }
        break;

        case 'u':
        {
        }
        break;

        case 'c':
        {
            int i = 0;

            printf("ARG[0] = %s\n", request_args[0]);

            while (request_args[i] != NULL)
            {

                char abs_path[MAXFILENAME];

                realpath(request_args[i], abs_path);

                PATH_CHECK(abs_path);

                printf("\n-c command execution:\n");

                if (removeFile(abs_path) == 0)
                {
                    printf("\n<<<SUCCESFULLY REMOVED THE FILE>>>\n");
                }
                else
                {
                    /* error handling */
                    printf("\n<<<ERROR WHILE REMOVING DUE TO (%s)>>>\n", translate_error_code(errno));
                }

                i++;
            }
        }
        break;

        default:
            break;
        }

        printf("(request performed) dequeuing the request...\n");
        LL_dequeue(request_commands);

        free(request_args);

        if (request_commands->head == NULL)
            printf("Request list is empty\n");
        else
            print_requests(request_commands);

        current_request_node = request_commands->head;
    }

    return 0;
}

Client_setup apply_setup(LList config_commands)
{
    Client_setup curr_setup;

    errno = 0;

    curr_setup.socket_pathname = NULL;
    curr_setup.dirname_buffer = NULL;
    curr_setup.ejected_buffer = NULL;
    curr_setup.req_time_interval = 0;
    curr_setup.op_log = 0;

    if (LL_contains_key(config_commands, "f") == TRUE)
    {
        curr_setup.socket_pathname = (char *)LL_get_by_key(config_commands, "f");
    }
    else
    {
        fprintf(stderr, "must specify socket path using -f option.\n");
        errno = MISSING_SOCKET_NAME;

        return curr_setup;
    }

    if (LL_contains_key(config_commands, "p") == TRUE)
    {
        curr_setup.op_log = atol((char *)LL_get_by_key(config_commands, "p"));
    }

    if (LL_contains_key(config_commands, "t") == TRUE)
    {
        curr_setup.req_time_interval = atol((char *)LL_get_by_key(config_commands, "t"));
    }

    if (LL_contains_key(config_commands, "d") == TRUE)
    {
        curr_setup.dirname_buffer = (char *)LL_get_by_key(config_commands, "d");
    }

    if (LL_contains_key(config_commands, "D") == TRUE)
    {
        curr_setup.ejected_buffer = (char *)LL_get_by_key(config_commands, "D");
    }

    return curr_setup;
}

int validate(LList configs, LList requests)
{
    /* t needs to be a valid integer */
    if (LL_contains_key(configs, "t") == TRUE)
    {
        char *endptr = NULL;
        char *t_value = (char *)LL_get_by_key(configs, "t");

        errno = 0;
        int t = strtol(t_value, &endptr, 10);

        if ((t == 0 && errno != 0) || (t == 0 && endptr == t_value))
        {
            fprintf(stderr, "invalid -t arg.\n");
            errno = 0;
            return NAN_INPUT_ERROR;
        }
        else
        {
            if (t < 0)
            {
                fprintf(stderr, "-t arg must be positive.\n");
                return INVALID_NUMBER_INPUT_ERROR;
            }
        }
    }

    /* R arg (if exists) must be an integer */
    int R_position = LL_contains_key(requests, "R");
    if (R_position == TRUE)
    {
        // Usato per gestire gli errori in strtol
        char *endptr = NULL;
        Request *R_value = (Request *)LL_get_by_key(requests, "R");

        errno = 0;
        int R = strtol(R_value->arguments, &endptr, 10);

        if ((R == 0 && errno != 0) || (R == 0 && endptr == R_value->arguments))
        {
            fprintf(stderr, "Il parametro di -R non è valido.\n");
            errno = 0;
            return NAN_INPUT_ERROR;
        }
    }

    /* -d needs -R or -r */
    if (LL_contains_key(requests, "d") == TRUE)
    {
        if (!(LL_contains_key(requests, "r") || LL_contains_key(requests, "R")))
        {
            fprintf(stderr, "L'opzione -d non puo' essere usata senza -r o -R.\n");
            return INCONSISTENT_INPUT_ERROR;
        }
    }

    /* -D needs -W or -w */
    if (LL_contains_key(requests, "D") == TRUE)
    {
        if (!(LL_contains_key(requests, "w") || LL_contains_key(requests, "W")))
        {
            fprintf(stderr, "L'opzione -D non puo' essere usata senza -w o -W.\n");
            return INCONSISTENT_INPUT_ERROR;
        }
    }

    return 0;
}

char **parse_comma_args(char *arguments)
{
    int args_counter = 1;
    int i = 0;

    // Calcolo il numero di argomenti, ogni virgola è un argomento in più
    while (arguments[i] != '\0')
    {
        if (arguments[i] == ',')
        {
            args_counter++;
        }

        i++;
    }
    args_counter++;

    // Alloco correttamente il vettore di argomenti
    char **strings = malloc(sizeof(char *) * args_counter);
    int j = 0;

    // Prendo la prima stringa
    strings[0] = strtok(arguments, ",");
    j++;

    // Prendo le restanti
    while ((strings[j] = strtok(NULL, ",")) != NULL)
        j++;

    return strings;
}

void print_config_node(Node *n_to_print)
{
    char *string = (char *)(n_to_print->data);

    printf("Key: %s, value: %s\n", n_to_print->key, string);
}

void print_request_node(Node *n_to_print)
{
    Request *to_print = (Request *)n_to_print->data;

    if (to_print->code != 'p')
        printf("Op: %c, args: %s\n", to_print->code, to_print->arguments);
}

void check_argc(int argcount)
{
    if (argcount == 1)
    {
        printf("You have to insert at least an argument to start\n");
        exit(EXIT_FAILURE);
    }
}

void print_help(char **_argv)
{
    printf("\n => usage: %s\n   -h <help> -f <filename> -w <dirname[,n=0]>\n   -W <file1[,file2]> -r <file1,[,file2] -R <int>\n   -d <dirname> -t <time> -l <file1[,file2]>\n   -u <file1[,file2]> -c <file1[,file2]> -p\n", _argv[0]);
}

void print_setup(Client_setup setup)
{
    printf("\n---- CLIENT SETUP ----\n");
    printf("==> socket_name = %s\n", setup.socket_pathname);
    if (setup.dirname_buffer != NULL)
        printf("==> read_dir = %s\n", setup.dirname_buffer);
    printf("==> request_rate = %d\n", setup.req_time_interval);
    printf("==> op_log = %d\n", setup.op_log);
}

void print_requests(LList *request_commands)
{
    LL_print(*request_commands, "REQUESTS");
}

int lsR(const char *dirname, int *n)
{
    /* base case */
    if (n == 0)
        return 0;

    struct stat statbuf;

    int r;

    if ((r = stat(dirname, &statbuf)) == -1)
    {
        if (errno == 2)
            return NOSUCHFILE_ORDIR;
        else
        {
            perror("fatal error\n");
            exit(EXIT_FAILURE);
        }
    }

    /* check if dirname is actually a directory */
    if (S_ISDIR(statbuf.st_mode))
    {
        DIR *dir;

        if ((dir = opendir(dirname)) == NULL)
        {
            printf("Error while opening %s dir\n", dirname);
            perror("opendir");
            return FS_ERR;
        }

        struct dirent *file;

        while ((errno = 0, file = readdir(dir)) != NULL && (*n != 0))
        {
            if (strcmp(file->d_name, ".") != 0)
            {
                if (strcmp(file->d_name, "..") != 0)
                {
                    char filename[MAXFILENAME];
                    int len1 = strlen(dirname);
                    int len2 = strlen(file->d_name);

                    if ((len1 + len2 + 2) > MAXFILENAME)
                    {
                        fprintf(stderr, "MAXFILENAME too small\n");
                        return FS_ERR;
                    }

                    strncpy(filename, dirname, MAXFILENAME - 1);
                    strncat(filename, "/", MAXFILENAME - 1);
                    strncat(filename, file->d_name, MAXFILENAME - 1);

                    lsR(filename, n);
                }
            }
        }

        closedir(dir);

        return 0;
    }
    else
    {
        if (S_ISREG(statbuf.st_mode))
        {
            char abs_path[MAXFILENAME];

            int open_res = openFile(realpath(dirname, abs_path), (O_CREATE | O_LOCK));

            /* API Call */
            if (open_res == 0)
            {

                if (writeFile(realpath(dirname, abs_path), c_setup.ejected_buffer) == 0)
                {
                    (*n)--;
                    if (closeFile(realpath(dirname, abs_path)) == -1)
                        printf("\n<<<CLOSE FAILED DUE TO (%s)>>>\n", translate_error_code(errno));

                    printf("closed successfully\n");
                }
                else
                    printf("\n<<<WRITE FILE ERROR>>>\n");
            }
            else
            {
                printf("\n<<<OPEN FAILED DUE TO (%s)>>>\n", translate_error_code(errno));
            }

            return 0;
        }
        else
        {
            return DIRNAME_NOT_FOLDER;
        }
    }
}

char *translate_error_code(int code)
{
    if (code == FAILED_FILE_SEARCH)
        return "failed file search";
    if (code == FILE_IS_LOCKED)
        return "file is locked";

    return "unknown error code";
}

void manage_request_option(char **opt_id, int opt, Request **_req, LList *reqs, char *_optarg)
{

    sprintf((*opt_id), "%c", opt);

    (*_req)->code = opt;
    (*_req)->arguments = (char *)_optarg;

    LL_enqueue(reqs, (void *)(*_req), (*opt_id));

    (*_req) = malloc(sizeof(Request));
    (*opt_id) = malloc(sizeof(char) * 2);
}

void manage_config_option(char **opt_id, char **opt_arg_value, int opt, LList *configs, char *_optarg)
{
    sprintf((*opt_id), "%c", opt);
    sprintf((*opt_arg_value), "%s", _optarg);

    LL_enqueue(configs, (void *)(*opt_arg_value), (*opt_id));

    (*opt_id) = malloc(sizeof(char) * 2);
    (*opt_arg_value) = malloc(sizeof(char) * 50);
}