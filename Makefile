TARGET = julia

CC = gcc
SRCS = base/buffer.c base/list.c\
		base/map.c base/pool.c\
		base/string.c base/vector.c\
	   	config.c connection.c juson.c parse.c\
		request.c response.c server.c util.c
		 
CFLAGS = -O2 -g -std=c11 -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE -I./

OBJS_DIR = build/
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	-rm -rf $(TARGET) $(OBJS)
