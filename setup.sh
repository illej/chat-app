#!/bin/bash

wget http://enet.bespin.org/download/enet-1.3.15.tar.gz

tar -xzf enet-1.3.15.tar.gz

cd enet-1.3.15
sudo ./configure && sudo make && sudo make install
cd ..

echo 'Cleaning up'

sudo rm -rf enet-1.3.15
rm enet-1.3.15.tar.gz

echo 'Building app'

export LD_LIBRARY_PATH=/usr/local/lib
gcc -pthread chat_server.c -o chat.srv -lenet
gcc -pthread chat_client.c -o chat.cl -lenet

echo 'Done'
