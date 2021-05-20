#!/bin/bash

valgrind ./client -f /tmp/server_sock1 -W config.txt
sleep 0.2
valgrind ./client -f /tmp/server_sock1 -w test_folder
sleep 0.2
valgrind ./client -f /tmp/server_sock1 -W _make.sh
sleep 0.2
valgrind ./client -f /tmp/server_sock1 -d read_files -R
sleep 0.2
valgrind ./client -f /tmp/server_sock1 -r test_folder/test_04.txt
sleep 0.2
valgrind ./client -f /tmp/server_sock1 -c test_folder/test_04.txt,test_folder/test_03.txt
sleep 0.2
valgrind ./client -f /tmp/server_sock1 -c test_folder/test_01.txt