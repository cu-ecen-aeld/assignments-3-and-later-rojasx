CC ?= $(CROSS_COMPILER)gcc
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -pthread -lrt
TARGET ?= aesdsocket

all: aesdsocket

aesdsocket: aesdsocket.c queue.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -rf *.o aesdsocket