#!/bin/bash

echo "Test 01"

valgrind --leak-check=full --show-leak-kinds=all ./server config1.txt &
pid=$!

sleep 3s

./client -f /tmp/server_sock -W test_src/images/orange.jpg -w test_src/txts -t 200 -p
./client -f /tmp/server_sock -r test_src/txts/test_01.txt -t 200 -p
./client -f /tmp/server_sock -c test_src/txts/test_01.txt,test_src/txts/test_03.txt -t 200 -p
./client -f /tmp/server_sock -R -d read_files -p
./client -h 

sleep 1s

kill -s SIGHUP $pid
wait $pid