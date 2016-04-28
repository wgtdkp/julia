TARGET = julia

CC = gcc
SRCS = buffer.c connection.c request.c response.c server.c string.c util.c
CFLAGS = -pg -std=c11 -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE 

OBJS_DIR = build/
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) -pg -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	-rm -rf $(TARGET) $(OBJS)
