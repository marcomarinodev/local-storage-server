server_exe : server.o config_parser.o pthread_custom.o queue.o
	gcc server.o config_parser.o pthread_custom.o queue.o -g -o server_exe -lpthread

server.o : server.c util.h
	gcc -Wall -pedantic -g -c server.c -lpthread

pthread_custom.o : pthread_custom.c pthread_custom.h
	gcc -Wall -pedantic -g -c pthread_custom.c -lpthread

config_parser.o : config_parser.c config_parser.h
	gcc -Wall -pedantic -g -c config_parser.c

queue.o : queue.c queue.h
	gcc -Wall -pedantic -g -c queue.c

cleanall:
	@echo "Removing Garbage"
	-rm -f *.o core *.~
	-rm /tmp/server_sock1


