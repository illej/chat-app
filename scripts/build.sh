#!/bin/bash

mkdir -p build

gcc -pthread src/chat_server.c -o build/chat.srv -lenet
gcc -pthread src/chat_client.c -o build/chat.cl -lenet
