TARGET = julia

CC = gcc
SRCS = base/buffer.c base/list.c\
		base/map.c base/pool.c\
		base/string.c base/vector.c\
	   	config.c connection.c juson.c parse.c\
		request.c response.c server.c util.c
		 
CFLAGS = -std=c11 -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE -I./

OBJS_DIR = build/
OBJS = $(SRCS:.c=.s)

$(TARGET): $(OBJS)
	gcc -o $@ $^

%.s: %.c
	$(CC) $(CFLAGS) -o $@ -S $<

.PHONY: clean

clean:
	-rm -rf $(TARGET) $(OBJS) *.s base/*.s

TEST_SRCS = base/buffer.c base/list.c\
		base/map.c base/pool.c\
		base/string.c base/vector.c\
	   	config.c connection.c juson.c parse.c\
		request.c response.c test.c util.c
TEST_OBJS = $(TEST_SRCS:.c=.s)

TEST: $(TEST_OBJS)
	gcc -o $@ $^


