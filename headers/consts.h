/* boolean values */
#define TRUE 1
#define FALSE 0

/* opening modes */
#define NO_FLAGS -1
#define O_CREATE 1
#define O_LOCK 2

/* strings limits */
#define MAX_CHARACTERS 900000
#define MAX_PATHNAME 300
#define MAX_LOGFILENAME 80
#define MAX_TIMESTRING 100

/* linked list errors */
#define INVALID_INDEX -1
#define NO_DATA -2
#define EMPTY_LIST -3
#define NOT_FOUND -4

/* Response codes */
/* failure codes */
#define FAILED_FILE_SEARCH 404
#define STRG_OVERFLOW 440
#define WRITE_FAILED 430
#define IS_ALREADY_OPEN 420
#define IS_ALREADY_CLOSED 450
#define FILE_IS_LOCKED 410
#define FILE_ALREADY_EXISTS 490
#define FILE_NOT_OPENED 500
#define NOT_AUTH 510

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
