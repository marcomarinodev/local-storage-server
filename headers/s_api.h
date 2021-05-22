#if !defined(S_API_H_)
#define S_API_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>

#define UNIX_PATH_MAX 108

/* opening modes */
#define NO_FLAGS -1
#define O_CREATE 1
#define O_LOCK 2

/* strings limits */
#define MAX_CHARACTERS 900000
#define MAX_PATHNAME 300

/* Response codes */
/* failure codes */
#define FAILED_FILE_SEARCH 404
#define STRG_OVERFLOW 440
#define WRITE_FAILED 430
#define IS_ALREADY_OPEN 420
#define IS_ALREADY_CLOSED 450
#define FILE_IS_LOCKED 410
#define FILE_ALREADY_EXISTS 490

/* success codes */
#define OPEN_SUCCESS 200
#define WRITE_SUCCESS 230
#define O_CREATE_SUCCESS 260
#define READ_SUCCESS 240
#define CLOSE_FILE_SUCCESS 210
#define REMOVE_FILE_SUCCESS 220
#define CLOSECONN_SUCCESS 270
#define APPEND_FILE_SUCCESS 280

/* request codes */
#define OPEN_FILE_REQ 10
#define READ_FILE_REQ 11
#define WRITE_FILE_REQ 12
#define APPEND_FILE_REQ 13
#define LOCK_FILE_REQ 14
#define UNLOCK_FILE_REQ 15
#define CLOSE_FILE_REQ 16
#define REMOVE_FILE_REQ 17
#define READN_FILE_REQ 18
#define CLOSECONN 19

#define SYSCALL(r, c, e) \
    if ((r = c) == -1)   \
    {                    \
        perror(e);       \
        exit(errno);     \
    }

typedef struct request_tosend
{
    pid_t calling_client;
    int cmd_type;
    char pathname[MAX_PATHNAME];
    int flags;     /* -1 there is no flags */
    long int size; /* -1 there is no file to send (in bytes) */
    char content[MAX_CHARACTERS];
    size_t content_size;
    int fd_cleint;
} ServerRequest;

typedef struct response
{
    char path[MAX_PATHNAME];
    char content[MAX_CHARACTERS];
    size_t content_size;
    int code;
} Response;

struct pthread_arg_struct
{
    int fd_c;
    int worker_index;
};

/**
 * Viene aperta una connessione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la
 * richiesta di connessione, la connessione da parte del client viene ripetuta dopo ‘msec’ millisecondi e fino allo
 * scadere del tempo assoluto ‘abstime’ specificato come terzo argomento. Ritorna 0 in caso di successo, -1 in caso
 * di fallimento, errno viene settato opportunamente.
*/
int openConnection(const char *sockname, int msec, const struct timespec abstime);

/**
 * Chiude la connessione AF_UNIX associata al socket file sockname. Ritorna 0 in caso di successo, -1 in caso di
 * fallimento, errno viene settato opportunamente.

*/
int closeConnection(const char *sockname);

/**
 * Richiesta di apertura o di creazione di un file. La semantica della openFile dipende dai flags passati come secondo
 * argomento che possono essere O_CREATE ed O_LOCK. Se viene passato il flag O_CREATE ed il file esiste già
 * memorizzato nel server, oppure il file non esiste ed il flag O_CREATE non è stato specificato, viene ritornato un
 * errore. In caso di successo, il file viene sempre aperto in lettura e scrittura, ed in particolare le scritture possono
 * avvenire solo in append. Se viene passato il flag O_LOCK (eventualmente in OR con O_CREATE) il file viene
 * aperto e/o creato in modalità locked, che vuol dire che l’unico che può leggere o scrivere il file ‘pathname’ è il
 * processo che lo ha aperto. Il flag O_LOCK può essere esplicitamente resettato utilizzando la chiamata unlockFile,
 * descritta di seguito.
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
*/
int openFile(const char *pathname, int flags);

/**
 * Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul file dopo la closeFile falliscono.
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
*/
int closeFile(const char *pathname);

/**
 * Scrive tutto il file puntato da pathname nel file server. Ritorna successo solo se la precedente operazione,
 * terminata con successo, è stata openFile(pathname, O_CREATE| O_LOCK). Se ‘dirname’ è diverso da NULL, il
 * file eventualmente spedito dal server perchè espulso dalla cache per far posto al file ‘pathname’ dovrà essere
 * scritto in ‘dirname’; Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.

*/
int writeFile(const char *pathname, const char *dirname);

/**
 * Legge tutto il contenuto del file dal server (se esiste) ritornando un puntatore ad un'area allocata sullo heap nel
 * parametro ‘buf’, mentre ‘size’ conterrà la dimensione del buffer dati (ossia la dimensione in bytes del file letto). In
 * caso di errore, ‘buf‘e ‘size’ non sono validi. Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene
 * settato opportunamente.
*/
int readFile(const char *pathname, void **buf, size_t *size);

/**
 * Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella directory ‘dirname’ lato client. Se il server
 * ha meno di ‘N’ file disponibili, li invia tutti. Se N<=0 la richiesta al server è quella di leggere tutti i file
 * memorizzati al suo interno. Ritorna un valore maggiore o uguale a 0 in caso di successo (cioè ritorna il n. di file
 * effettivamente letti), -1 in caso di fallimento, errno viene settato opportunamente.
*/
int readNFiles(int N, const char *dirname);

/**
 * Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul file dopo la closeFile falliscono.
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente. 
*/
int removeFile(const char *pathname);

/**
 * In caso di successo setta il flag O_LOCK al file. Se il file era stato aperto/creato con il flag O_LOCK e la
 * richiesta proviene dallo stesso processo, oppure se il file non ha il flag O_LOCK settato, l’operazione termina
 * immediatamente con successo, altrimenti l’operazione non viene completata fino a quando il flag O_LOCK non
 * viene resettato dal detentore della lock. L’ordine di acquisizione della lock sul file non è specificato. Ritorna 0 in
 * caso di successo, -1 in caso di fallimento, errno viene settato opportunamente
*/
int lockFile(const char *pathname);

/**
 * Resetta il flag O_LOCK sul file ‘pathname’. L’operazione ha successo solo se l’owner della lock è il processo che
 * ha richiesto l’operazione, altrimenti l’operazione termina con errore. Ritorna 0 in caso di successo, -1 in caso di
 * fallimento, errno viene settato opportunamente.
*/
int unlockFile(const char *pathname);

/**
 * Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’. L’operazione di append
 * nel file è garantita essere atomica dal file server. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito
 * dal server perchè espulso dalla cache per far posto ai nuovi dati di ‘pathname’ dovrà essere scritto in ‘dirname’;
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
*/
int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname);

long int find_size(const char *pathname);

ssize_t readn(int fd, void *ptr, size_t n);

ssize_t writen(int fd, void *ptr, size_t n);

#endif