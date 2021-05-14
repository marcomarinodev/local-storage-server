#!/bin/bash

for ((i=0;i<100;i+=1)); do
    ./client -f /tmp/server_sock1 -W config.txt
done

