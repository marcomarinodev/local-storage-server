#!/bin/bash

# testing the active connections queue in server.c
valgrind ./client -f /tmp/server_sock -D expelled_dir -w test_src/images &&
valgrind ./client -f /tmp/server_sock -D expelled_dir -W test_src/pdfs/ex1.pdf