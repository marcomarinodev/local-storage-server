USAGE:

	"make"
	"./server" to run the server
	"bash run_multiclient.sh" to spawn 100 clients
	"make cleanall" at the end

WHAT WAS DONE:

1)	Initial Makefile management with dynamic libraries
2)	Configuration Server parsing by server itself
3)	The server can handle multiple client requests with a fixed
	number of worker threads
4)	Worker threads use a Queue data structure in mutual exclusion
	to manipulate the requests
5)	A small helper was created to catch more arguments in an option
	(client side)

NOTES:

Once the server has done its job, a SIGINT signal stop brutally the server,
so that the pthread does not release their own memory inside the heap (mem leak).
But this is going to be fixed once I am going to worry about ending signals.

WHAT TO DO:

-	Client Interface
-	Storage data structure
-	API Implementation
-	Handle Locks on file inside de file storage
-	Implement the FIFO in order to make space to 
	a new incoming file
-	Signal Handler (SIGINT, SIGQUIT, SIGHOP)
-	Deployment Testing