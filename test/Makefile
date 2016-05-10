TARGET = client

CC = gcc
SRCS = client.c
CFLAGS = -std=c11 -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE 

OBJS_DIR = build/
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) -fno-common -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	-rm -rf $(TARGET) $(OBJS)
