#!/bin/bash

mkdir -p build

BIN_PATH=/usr/local/lib # want it to be 'build'

gcc -pthread src/chat_server.c -Wl,-rpath $BIN_PATH -o build/chat.srv -lenet
gcc -pthread src/chat_client.c -Wl,-rpath $BIN_PATH -o build/chat.cl -lenet
