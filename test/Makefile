TARGET = test

CC = gcc
SRCS = test.c ../base/list.c ../base/vector.c ../base/pool.c ../base/heap.c
INC = -I/usr/include -I../ -I../base
CFLAGS = -g -std=c11 -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE $(INC)

OBJS_DIR = build/
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) -fno-common -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	-rm -rf $(TARGET) $(OBJS)
