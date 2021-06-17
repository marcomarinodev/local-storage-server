.PHONY : test1 test2

CC = gcc
STDC = -std=c99
STD_FLAGS = -Wall -pedantic
THREAD_FLAGS = -lpthread
INCLUDES = -I./headers

O_FOLDER = build/objs
DEPS_FOLDER = sources/Dependencies

client_deps = sources/client.c libs/libds.so libs/libapi.so
server_deps = sources/server.c libs/libserv.so

all: client server

client: $(client_deps)
	$(CC) $(INCLUDES) $(STD_FLAGS) sources/client.c -g -o client -Wl,-rpath,./libs -L ./libs -lds -lapi $(THREAD_FLAGS)

server: $(server_deps)
	$(CC) $(INCLUDES) $(STD_FLAGS) sources/server.c -g -o server -Wl,-rpath,./libs -L ./libs -lserv -lapi $(THREAD_FLAGS)

# Libraries
libs/libds.so: $(O_FOLDER)/queue.o $(O_FOLDER)/linked_list.o $(O_FOLDER)/pthread_custom.o $(O_FOLDER)/utility.o
	$(CC) -shared -o libs/libds.so $^

libs/libserv.so: $(O_FOLDER)/queue.o $(O_FOLDER)/linked_list.o $(O_FOLDER)/ht.o $(O_FOLDER)/pthread_custom.o $(O_FOLDER)/utility.o $(O_FOLDER)/config_parser.o $(O_FOLDER)/doubly_ll.o
	$(CC) -shared -o libs/libserv.so $^

$(O_FOLDER)/utility.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/utility.c -g -c -fPIC -o $@

$(O_FOLDER)/config_parser.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/config_parser.c -g -c -fPIC -o $@

$(O_FOLDER)/pthread_custom.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/pthread_custom.c -g -c -fPIC -o $@

$(O_FOLDER)/queue.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/queue.c -g -c -fPIC -o $@

$(O_FOLDER)/doubly_ll.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/doubly_ll.c -g -c -fPIC -o $@

$(O_FOLDER)/linked_list.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/linked_list.c -g -c -fPIC -o $@

$(O_FOLDER)/ht.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/ht.c -g -c -fPIC -o $@

libs/libapi.so: $(O_FOLDER)/s_api.o $(O_FOLDER)/utility.o
	$(CC) -shared -o libs/libapi.so $^

$(O_FOLDER)/s_api.o:
	$(CC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/s_api.c -g -c -fPIC -o $@

$(O_FOLDER)/utility.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/utility.c -g -c -fPIC -o $@

cleanall:
	@echo "Garbage Removal"
	-rm -f read_files/*
	-rm -f expelled_dir/*
	-rm -f build/objs/*.o
	-rm -f libs/*.so
	-rm /tmp/server_sock
	-rm -rf ServerLogs

# tests
test1: client server
	./tests/test01.sh 

test2: client server
	./tests/test02.sh
