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
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define ABORT_ON(cond, msg) {   \
    if (cond) {                 \
        perror(msg);            \
        abort();                \
    }                           \
};


int startup(uint16_t port)
{
    static const char* host = "127.0.0.1";
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ABORT_ON(fd == -1, "socket");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    int success = inet_pton(AF_INET, host, &addr.sin_addr);
    ABORT_ON(success <= 0, "inet_pton");

    struct linger ling = {
        .l_onoff = 1,
        .l_linger = 10
    };
    int err = setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    ABORT_ON(err != 0, "setsockopt");
    err = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    ABORT_ON(err != 0, "connect");

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
    return len;
}

int main(int argc, char* argv[])
{
    if (argc < 3)
        return 0;
    int fd = startup(atoi(argv[1]));
    
    FILE* file = fopen(argv[2], "r");
    if (file == NULL) {
        printf("open file: %s failed.", argv[2]);
        return -1;
    }
    for (; ;) {
        int ch = getc(file);
        if (ch == EOF)
            break;
        assert(1 == send(fd, &ch, 1, 0));
        printf("%c", ch);
        fflush(stdout);
        usleep(20 * 1000);
    }
    
    for (; ;) {
        int ch;
        int len = recv(fd, &ch, 1, 0);
        if (len != 1)
            break;
        printf("%c", ch);
        fflush(stdout);
    }
    
    close(fd);
    
    fclose(file);
    return 0;
}