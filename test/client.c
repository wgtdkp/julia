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
#include <stdarg.h>
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
    static const char* host = "127.0.0.1";
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    EXIT_ON(fd == -1, "socket");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    int success = inet_pton(AF_INET, host, &addr.sin_addr);
    assert(success > 0);

    struct linger ling = {
        .l_onoff = 1,
        .l_linger = 10
    };
    int err = setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    EXIT_ON(err != 0, "setsockopt");
    err = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    EXIT_ON(err != 0, "connect");

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
    if (argc < 2)
        return 0;
    int fd = startup(atoi(argv[1]));

    char str[] = "GET / HTTP/1.1         \r\n"
                 "Host: localhost:8008     \r\n"
                 "Connection: keep-alive       \r\n"
                 "Cache-Control: max-age=0         \r\n"
                 "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8     \r\n"
                 "Upgrade-Insecure-Requests: 1       \r\n"
                 "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Ubuntu Chromium/49.0.2623.108 Chrome/49.0.2623.108 Safari/537.36     \r\n"
                 "Accept-Encoding: gzip, deflate, sdch      \r\n"
                 "Accept-Language: zh-CN,zh;q=0.8,ko;q=0.6           \r\n";
    for (char* p = str; *p != 0; p++) {
        assert(1 == send(fd, p, 1, 0));
        printf("%c", *p);
        fflush(stdout);
        //usleep(20 * 1000);
    }

    //shutdown(fd, SHUT_WR);
    //usleep(1000);
    close(fd);
    return 0;
}