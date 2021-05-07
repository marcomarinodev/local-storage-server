#include <stdlib.h>

/**
 * It opens AF_UNIX connection to the socket named sockname. If the server does not
 * accept the request immediately, the client try to reconnect after msec. If the server
 * did not accept the request of the client in the allotted time (abstime), it returns -1 (errno).
 * Otherwise the connection is stable and it returns 0.
*/
int openConnection(const char* sockname, int msec, const struct timespec abstime);

/**
 * It closes AF_UNIX connection to the socket named sockname. It returns 0 in success, -1 
 * otherwise.
*/
int closeConnection(const char* sockname);

/* TODO: - IMPLEMENT THESE FUNCTIONS
int openFile(const char* pathname, int flags);

int readFile(const char* pathname, void** buf, size_t* size);

int writeFile(const char* pathname, const char* dirname);

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

int closeFile(const char* pathname);

int removeFile(const char* pathname);
*/