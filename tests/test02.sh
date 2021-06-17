#!/bin/bash

echo "Test 02"

./server configs/config2.txt &
pid=$!

sleep 2s

# dim(test_src/images/pc.jpg) + dim(test_src/pdfs/ex1.pdf) = 1MB

# size overflow
./client -f /tmp/server_sock -D expelled_dir -W test_src/images/pc.jpg -p
./client -f /tmp/server_sock -D expelled_dir -W test_src/pdfs/ex1.pdf -p
./client -f /tmp/server_sock -D expelled_dir -W test_src/orange.jpg -p

# cardinality overflow
for i in {1..9}; do
    ./client -f /tmp/server_sock e -W test_src/txts/test_0$i.txt -p
done

./client -f /tmp/server_sock -D expelled_dir e -W test_src/txts/test_10.txt -p
./client -f /tmp/server_sock -d read_files -R

sleep 2s

kill -s SIGHUP $pid
wait $pid
