
CC=gcc
DEFS =-D_XOPEN_SOURCE -D_GNU_SOURCE
julia:
	${CC} -std=c11 -pthread  ${DEFS} server.c -o julia
client:
	${CC} -std=c11 ${DEFS} client.c -o client
test:
	${CC} -std=c11 ${DEFS} ./test/test.c -o ./test/test
all:
	make server & make client
clean:
	rm julia client