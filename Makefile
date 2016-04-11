
CC=gcc
server:
	${CC} -std=c11 -pthread  server.c -o server
client:
	${CC} -std=c11 client.c -o client
all:
	make server & make client
clean:
	rm server client