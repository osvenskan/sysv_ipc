#!/usr/bin/env bash

gcc -Wall -c -o md5.o md5.c
gcc -Wall -c -o utils.o utils.c
gcc -Wall -L. md5.o utils.o -o premise premise.c
gcc -Wall -L. md5.o utils.o -o conclusion conclusion.c

