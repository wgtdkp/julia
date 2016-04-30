#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define EXIT_ON(cond, msg)  \
do {                        \
    if (cond)               \
        perror(msg);        \
} while (0)



int startup(unsigned short port)
{
    static const char* host = "localhost";
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    EXIT_ON(fd == -1);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    int success = inet_pton(AF_INET, host, &addr.sin_addr);
    assert(success > 0);
    int err = connect(fd, (struct sockaddr*)addr, sizeof(addr));
    EXIT_ON(err != 0);

    return fd;
}

int my_send(int fd, const char* format, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    assert(len >= 0);
    write(fd, buffer, len);
    va_end(args);
    fclose(log_file);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
        return 0;
    int fd = startup(atoi(argv[1]));

    char str[] = "GET /hello HTTP/1.1\r\n";
    for (char* p = str; *p != 0; p++) {
        send(fd, p, 1);
        usleep(100 * 1000);
    }
    return 0;
}