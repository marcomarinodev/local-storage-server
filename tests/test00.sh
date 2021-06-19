#!/bin/bash

echo "Test 00"

valgrind ./server configs/config1.txt &
pid=$!

sleep 3s

./client -f /tmp/server_sock -W test_src/images/orange.jpg -t 10000 
./client -f /tmp/server_sock -w test_src/txts -t &
kill -s SIGHUP $pid
./client -f /tmp/server_sock -d read_files -R
./client -h 

sleep 1s

echo $pid

wait $pid