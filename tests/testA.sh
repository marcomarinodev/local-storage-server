#!/bin/bash

# testing the active connections queue in server.c
valgrind ./client -f /tmp/server_sock1 -W test_src/images/orange.jpg &
valgrind ./client -f /tmp/server_sock1 -W test_src/test_01.txt &
valgrind ./client -f /tmp/server_sock1 -W test_src/pdfs/ex1.pdf &&
valgrind ./client -f /tmp/server_sock1 -d read_files -R