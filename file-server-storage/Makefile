.PHONY : test1 test2

CC = gcc
STDC = -std=c99
STD_FLAGS = -Wall -pedantic
THREAD_FLAGS = -lpthread
INCLUDES = -I./headers

O_FOLDER = build/objs
DEPS_FOLDER = sources/Dependencies

client_deps = sources/client.c libs/libdeps_client.so
server_deps = sources/server.c libs/libdeps_server.so

client: $(client_deps)
	$(CC) $(INCLUDES) $(STD_FLAGS) sources/client.c -g -o client -Wl,-rpath,./libs -L ./libs -ldeps_client $(THREAD_FLAGS)

server: $(server_deps)
	$(CC) $(INCLUDES) $(STD_FLAGS) sources/server.c -g -o server -Wl,-rpath,./libs -L ./libs -ldeps_server $(THREAD_FLAGS)

# Libraries
libs/libdeps_client.so: $(O_FOLDER)/arg_parser.o  $(O_FOLDER)/queue.o $(O_FOLDER)/s_api.o
	$(CC) -shared -o libs/libdeps_client.so $^

libs/libdeps_server.so: $(O_FOLDER)/config_parser.o $(O_FOLDER)/pthread_custom.o  $(O_FOLDER)/queue.o
	$(CC) -shared -o libs/libdeps_server.so $^

$(O_FOLDER)/arg_parser.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/arg_parser.c -g -c -fPIC -o $@

$(O_FOLDER)/config_parser.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/config_parser.c -g -c -fPIC -o $@

$(O_FOLDER)/pthread_custom.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/pthread_custom.c -g -c -fPIC -o $@

$(O_FOLDER)/queue.o:
	$(CC) $(STDC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/queue.c -g -c -fPIC -o $@

$(O_FOLDER)/s_api.o:
	$(CC) $(INCLUDES) $(STD_FLAGS) $(DEPS_FOLDER)/s_api.c -g -c -fPIC -o $@

cleanall:
	@echo "Garbage Removal"
	-rm -f build/objs/*.o
	-rm -f /libs/*.so
	-rm /tmp/server_sock1