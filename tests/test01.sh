#!/bin/bash

echo "Test 01"

valgrind  ./server configs/config1.txt &
pid=$!

sleep 3s

./client -f /tmp/server_sock -W test_src/images/orange.jpg -w test_src/txts -t 200
./client -f /tmp/server_sock -r test_src/txts/test_01.txt -t 200
./client -f /tmp/server_sock -d read_files -R
./client -h 

sleep 1s

echo $pid
kill -s SIGHUP $pid
wait $pid