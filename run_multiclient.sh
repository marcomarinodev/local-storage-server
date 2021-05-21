#!/bin/bash

valgrind ./client -f /tmp/server_sock1 -D expelled_files -W configs/config.txt &&
valgrind ./client -f /tmp/server_sock1 -D expelled_files -w test_folder &&
valgrind ./client -f /tmp/server_sock1 -D expelled_files -W _make.sh &&
valgrind ./client -f /tmp/server_sock1 -d read_files -R &&
valgrind ./client -f /tmp/server_sock1 -r test_folder/test_04.txt &&
valgrind ./client -f /tmp/server_sock1 -c test_folder/test_04.txt,test_folder/test_03.txt &&
valgrind ./client -f /tmp/server_sock1 -c test_folder/test_01.txt
