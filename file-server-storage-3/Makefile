.PHONY : test1 test2

CC = gcc
STDC = -std=c99
STD_FLAGS = -Wall -pedantic
THREAD_FLAGS = -lpthread
INCLUDES = -I./headers

O_FOLDER = build/objs
DEPS_FOLDER = sources/Dependencies

client_deps = sources/client.c libs/libds.so libs/libapi.so
server_deps = sources/server.c libs/libds.so

all: client server

client: $(client_deps)
	$(CC) $(INCLUDES) $(STD_FLAGS) sources/client.c -g -o client -Wl,-rpath,./libs -L ./libs -lds -lapi $(THREAD_FLAGS)

server: $(server_deps)
	$(CC) $(INCLUDES) $(STD_FLAGS) sources/server.c -g -o server -Wl,-rpath,./libs -L ./libs -lds $(THREAD_FLAGS)

# Libraries
libs/libds.so: $(O_FOLDER)/queue.o $(O_FOLDER)/linked_list.o $(O_FOLDER)/icl_hash.o $(O_FOLDER)/pthread_custom.o $(O_FOLDER)/config_parser.o
	$(CC) -shared -o libs/libds.so $^

$(O_FOLDER)/config_parser.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/config_parser.c -g -c -fPIC -o $@

$(O_FOLDER)/pthread_custom.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/pthread_custom.c -g -c -fPIC -o $@

$(O_FOLDER)/queue.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/queue.c -g -c -fPIC -o $@

$(O_FOLDER)/linked_list.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/linked_list.c -g -c -fPIC -o $@

$(O_FOLDER)/icl_hash.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/icl_hash.c -g -c -fPIC -o $@

libs/libapi.so: $(O_FOLDER)/s_api.o
	$(CC) -shared -o libs/libapi.so $^

$(O_FOLDER)/s_api.o:
	$(CC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/s_api.c -g -c -fPIC -o $@

cleanall:
	@echo "Garbage Removal"
	-rm -f build/objs/*.o
	-rm -f libs/*.so
	-rm /tmp/server_sock1