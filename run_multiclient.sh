#!/bin/bash

valgrind ./client -f /tmp/server_sock1 -W config.txt
valgrind ./client -f /tmp/server_sock1 -w test_folder
valgrind ./client -f /tmp/server_sock1 -W _make.sh
valgrind ./client -f /tmp/server_sock1 -d read_files -R
valgrind ./client -f /tmp/server_sock1 -r test_folder/test_04.txt