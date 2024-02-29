SHELL := /usr/bin/env bash

all: aesdsocket

aesdsocket: aesdsocket.c 
	$(CC)  aesdsocket.c -o aesdsocket -Wall -Werror -g3 -O0 -pthread

.PHONY: clean

clean:
	@rm -rf *.o aesdsocket