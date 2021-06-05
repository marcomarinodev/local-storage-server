#!/bin/bash

# testing the active connections queue in server.c
valgrind ./client -f /tmp/server_sock -D expelled_dir -w test_src/images &
valgrind ./client -f /tmp/server_sock -D expelled_dir -W test_src/test_01.txt &
valgrind ./client -f /tmp/server_sock -D expelled_dir -W test_src/test_02.txt &
valgrind ./client -f /tmp/server_sock -D expelled_dir -W test_src/test_03.txt &
valgrind ./client -f /tmp/server_sock -D expelled_dir -W test_src/test_04.txt &&
valgrind ./client -f /tmp/server_sock -D expelled_dir -W test_src/test_05.txt &&
# valgrind ./client -f /tmp/server_sock -D expelled_dir -W test_src/pdfs/ex1.pdf &&
valgrind ./client -f /tmp/server_sock -d read_files -R