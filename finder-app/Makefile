CFLAGS=-Wall -Wextra -g3 -O0
CROSS_COMPILE=
CC=$(CROSS_COMPILE)gcc


# Name of the executable binary
TARGET=writer

# Source files
SRCS=writer.c

# Object files
OBJS=$(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
		$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
		$(CC) $(CFLAGS) -c -o $@ $<

clean:
		rm -f $(TARGET) $(OBJS)