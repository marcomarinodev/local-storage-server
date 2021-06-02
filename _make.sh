#!/bin/bash

clear
make cleanall
make all
clear
valgrind ./server configs/config.txt