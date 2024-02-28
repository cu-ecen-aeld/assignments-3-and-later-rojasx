SHELL := /usr/bin/env bash

all: aesdsocket

aesdsocket: aesdsocket.c 
	$(CC)  aesdsocket.c -o aesdsocket -g3

.PHONY: clean

clean:
	@rm -rf *.o aesdsocket