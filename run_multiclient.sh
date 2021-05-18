#!/bin/bash

valgrind ./client -f /tmp/server_sock1 -W config.txt
sleep 3
valgrind ./client -f /tmp/server_sock1 -w test_folder
sleep 3
valgrind ./client -f /tmp/server_sock1 -W _make.sh
sleep 3
valgrind ./client -f /tmp/server_sock1 -d read_files -R 4