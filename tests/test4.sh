#!/bin/bash

valgrind ./client -f /tmp/server_sock -D expelled_dir -W test_src/test_01.txt &&
valgrind ./client -f /tmp/server_sock -D expelled_dir -W test_src/test_02.txt &&
valgrind ./client -f /tmp/server_sock -D expelled_dir -W test_src/test_03.txt 