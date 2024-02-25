#!/bin/sh

# Written by Xavier Rojas
# Quick script to manage our daemon!

AESDSOCKET_PATH="$(realpath $(dirname $0))"/aesdsocket

case "$1" in
    start)
        echo "Starting aesdsocket daemon"
        # echo $AESDSOCKET_PATH
        # start-stop-daemon -S -n aesdsocket --startas /usr/bin/aesdsocket -- -d
        start-stop-daemon -S -n aesdsocket --startas "$AESDSOCKET_PATH" -- -d
        ;;
    stop)
        echo "Stopping aesdsocket daemon"
        # SIGTERM is default
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        # Provide usage
        echo "Usage: $0 {start|stop}"
        exit 1
        ;;
esac