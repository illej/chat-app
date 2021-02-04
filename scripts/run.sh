#!/bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

usage()
{
    echo "Usage:"
    echo "  ./run.sh server"
    echo "  ./run.sh client"
}

if [ $# -eq 1 ]; then
    mode=$1

    if [[ ! -e build ]]; then
        ./build.sh
    fi

    case $mode in
        server)
            ./build/chat.srv
            ;;
        client)
            ./build/chat.cl
            ;;
        *)
            usage
            ;;
    esac
else
    usage
fi
