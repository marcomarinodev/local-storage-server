USAGE:

	"make cleanall"
	"make all"
	"./server configuration.txt" to run the server
	"./client" with options to run the client

es: valgrind --leak-check=full ./client -f /tmp/server_sock -d appdata -w headers,2 -W config.txt -r config.txt,config.txt -R

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
6)	Storage data structure
7)	Implemen select on master/worker pattern
8)	openFile, readFile, writeFile, openConnection, readNFiles
9)	LRU politic
10)	Locks handling

NOTES:

Once the server has done its job, a SIGINT signal stop brutally the server,
so that the pthread does not release their own memory inside the heap (mem leak).
But this is going to be fixed once I am going to worry about ending signals.

WHAT TO DO:

-	End API implementation
-	Signal Handler (SIGINT, SIGQUIT, SIGHOP)
-	Solve possible mem leaks in server
-	Manage some exception
-	Deployment Testing
